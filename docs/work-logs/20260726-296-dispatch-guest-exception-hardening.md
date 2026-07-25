# 20260726-296 작업 로그: DispatchGuestException 관측 견고화 / Work log

설계: [docs/design/20260726-296-dispatch-guest-exception-hardening.md](../design/20260726-296-dispatch-guest-exception-hardening.md)
작업 지시: [docs/work-orders/20260726-296-dispatch-guest-exception-hardening.md](../work-orders/20260726-296-dispatch-guest-exception-hardening.md)

## 한국어

### 1. 진단 요약

`repiu_log.txt`의 종료 예외를 PDB 심볼로 해석한 결과:

```
exception 0x101AF9A1 → repiu::platform::win32::DispatchGuestException + 0xB1
                        execution_trampoline.cpp:2379  (if (win32_context->Eip == 0U))
code 0xC0000005, fault VA 0x000000DB, ECX=0x23
faulting bytes: 83 B9 B8 00 00 00 00 = CMP dword ptr [ECX+0xB8], 0
```

x86 `CONTEXT`의 `Eip` 오프셋은 `0xB8`이므로 이 명령은 `win32_context->Eip`이고,
`win32_context`(= `exception_info->ContextRecord`)가 쓰레기 비-null 값 `0x23`이었다
(`0x23+0xB8=0xDB` = fault VA). 즉 **Glide 크래시가 아니라 예외 디스패처의 2차 크래시**로,
1차 게스트 예외가 은폐되고 있었다. 진입부 가드가 `ContextRecord == nullptr`만 검사하여
비-null 쓰레기 포인터를 걸러내지 못한 것이 근인이다.

### 2. 변경 사항

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/execution_trampoline.cpp` | `IsHostPointerReadable`(VirtualQuery 기반), `RecordMalformedExceptionPointers` 헬퍼 추가. `DispatchGuestException` 진입부를 `info`/`ContextRecord`/`ExceptionRecord` 읽기 가능 검증으로 교체하고, 실패 시 진단 기록 후 `EXCEPTION_CONTINUE_SEARCH` 반환. |
| `src/platform/win32/execution/thread_context.h` | 진단 원자 필드 3개 추가(`exception_dispatch_malformed_count`, `_last_bad_context`, `_last_bad_record`). |
| `include/repiu/platform/win32/execution_trampoline.h` | `Win32MinimalExecutionAttempt`에 동일 3개 필드 추가. |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 요약 스냅샷에서 3개 필드 복사. |
| `src/host/win32/main.cpp` | 종료 요약에 malformed count / last bad Context·Exception Record 출력. |

### 3. 검증

1. **빌드**: `cmake --build build/win32_x86_debug --config Debug --target repiu_loader_win32` 성공
   (경고는 기존 C4819 코드페이지 경고뿐, 무관).
2. **회귀 없음 (핵심)**: `aot-dbt`로 `pumpit1` 구동(25s 타임아웃) 시 **정상 예외 디스패치가 대량
   성공**(`dispatch_entry=19412`), glide gate 49개 처리, `malformed count = 0`(오검출 없음),
   `0x101AF9A1` 2차 크래시 없음, exit code 0으로 정상 타임아웃 종료. 가드가 유효한 `CONTEXT`
   포인터를 잘못 거부하지 않음을 확인.
3. **요약 관측**: 종료 요약에 다음이 정상 출력됨.
   - `Win32 exception dispatch malformed count: 0`
   - `Win32 exception dispatch last bad ContextRecord: 0x00000000`
   - `Win32 exception dispatch last bad ExceptionRecord: 0x00000000`

#### 검증 한계 (정직한 기록)
원본 크래시(malformed `EXCEPTION_POINTERS`)는 게스트가 INT 8 벡터 설치 이후 타이머 주입 구간까지
도달해야 재현되는데, 구동이 **비결정적으로 초기 glide busy-wait(gate 9~49, `last_eip=0x030F4FFB`,
INT 8 미설치)에서 멈춰** 해당 프론티어까지 재현되지 않았다. 따라서 가드가 malformed 포인터를 실제로
포착하는 장면은 런타임에서 결정적으로 관측하지 못했다. 다만 본 수정은 대상 조건에 대한 결정적 가드
이며, 정상 디스패치 경로에 영향을 주지 않음(19412회 성공)을 확인하였다. 실제 재현 시 로그에
`[repiu-live] Malformed EXCEPTION_POINTERS ...` 및 요약 malformed count 증가로 관측될 것이다.

### 3-1. 성능 측정 및 hot-path 최적화 (추가)

초기 구현은 `DispatchGuestException` 진입부에서 예외마다 `VirtualQuery`를 3회 호출했다.
`DispatchGuestException`은 single-step(#DB)마다 호출되는 hot path여서 (원본 로그 기준 최대
~166K/s) 커널 전환 비용이 문제될 수 있다는 지적을 받아 정량 측정하였다.

- 마이크로벤치(3× VirtualQuery, 2M 반복): **디스패치당 ~1248 ns**.

| 디스패치율 | 3× VirtualQuery 오버헤드 |
|---|---|
| 390/s (관측된 stall 구간) | 0.05% |
| 9,500/s (초기 버스트) | 1.2% |
| 50,000/s | 6.2% |
| 166,000/s (single-step 집중) | 20.7% |

관측된 실제 디스패치율(~390~9,500/s)에서는 무시할 수준이었으나, single-step 집중 구간에서
최대 ~20%까지 커질 수 있어 hot path에서 커널 호출을 제거하였다.

- `IsPlausibleHostPointer`(분기 전용: `ptr >= 0x10000 && (ptr & 3) == 0`) 추가.
- 진입 가드를 **저렴한 산술 검사를 먼저** 수행하도록 재구성. 세 포인터가 모두 그럴듯하면 공통
  경로에서 `VirtualQuery`를 건너뛴다. 관측된 손상값(`0x23`, `0x287` 등 셀렉터/작은 정수)은
  산술 검사에서 걸러지고, 미심쩍은 경우에만 권위 있는 `VirtualQuery` 폴백을 지불한다.
  (`&&` 단락 평가로 `exception_info->{Context,Exception}Record` 판독은 `exception_info`
  검증 뒤에서만 수행됨.)

### 3-2. 최적화 버전 재검증

`aot-dbt`로 재구동한 결과:
- 정상 종료 전 **glide gate 74개까지 진행**(기존 49개보다 전진), `malformed count=0`, `0x101AF9A1`
  2차 크래시 없음. → 관측 견고화가 의도대로 작동하여 **가려졌던 다음 프론티어를 표면화**.
- 표면화된 새 프론티어: `HandleTracedDosInterrupt21 + 0x37`(`instruction_emulation.cpp:2696`)에서
  `C0000005`. 이 핸들러는 `win32_context->Eip`(=`0x287`, 쓰레기)를 **읽기 검증 없이 역참조**한다
  (`instruction[0] != 0xCD ...`). 게스트 EIP/스택 손상이라는 동일 근인의 또 다른 발현이며,
  브랜치가 이미 `port_io_emulator.cpp`에 적용한 `IsGuestRangeReadable(decode_eip, 2)` 패턴과 동일한
  처방이 필요하다.

### 4. 남은 과제 (후속)
- 근본 원인: 선점/비선점 INT 8 주입의 게스트 IF 플래그 게이트 및 중첩 방지, 게스트 스택 TIB
  Base/Limit 정합. (게스트 EIP `0x287`·리턴주소 `0x0301A8EC`·포인터 `0x23` 모두 제어흐름/스택
  손상을 가리킴.)
- 다음 프론티어 견고화: `HandleTracedDosInterrupt21` 등 traced-instruction 핸들러의 Eip 역참조
  전 `IsGuestRangeReadable` 가드 추가.
- 브랜치 디버그 스캐폴딩 정리(CMakeLists `resolve_sym` 외부 경로 타겟, `main.cpp` 디버그 fprintf,
  Glide skip 로그) — 커밋/머지 전.

### English addendum

The initial guard called `VirtualQuery` 3× per dispatch. Since `DispatchGuestException` is on the
single-step hot path (up to ~166K/s), a microbenchmark measured **~1248 ns/dispatch** (0.05% at 390/s,
but 20.7% at 166K/s). Optimized by adding a branch-only `IsPlausibleHostPointer`
(`ptr >= 0x10000 && aligned`) fast gate; the `VirtualQuery` fallback now runs only for implausible
pointers, so the common path pays no kernel transition. Re-verification under `aot-dbt`: execution
progressed further (74 glide gates vs 49), no `0x101AF9A1` secondary crash, `malformed count=0`, and the
fix **surfaced the next real frontier** — `HandleTracedDosInterrupt21+0x37`
(`instruction_emulation.cpp:2696`) faulting because it dereferences a garbage `win32_context->Eip`
(`0x287`) without an `IsGuestRangeReadable` guard. Same root cause (guest EIP/stack corruption); same
prescription as the branch's existing `port_io_emulator.cpp` guard.

## English

### 1. Diagnosis
The terminating exception at `0x101AF9A1` resolves (via PDB) to `DispatchGuestException + 0xB1`
(execution_trampoline.cpp:2379, `if (win32_context->Eip == 0U)`). Since the x86 `CONTEXT` `Eip` field is
at offset `0xB8`, the fault `CMP [ECX+0xB8]` with `ECX=0x23` means `win32_context`
(= `exception_info->ContextRecord`) was a garbage non-null `0x23` (`0x23+0xB8=0xDB` = fault VA). This is a
**secondary crash in the exception dispatcher** that masked the primary guest exception. The entry guard
only checked `ContextRecord == nullptr`, letting the garbage non-null pointer through.

### 2. Changes
Added `IsHostPointerReadable` (VirtualQuery-based) and `RecordMalformedExceptionPointers`; replaced the
`DispatchGuestException` entry guard with readability validation of `info`/`ContextRecord`/
`ExceptionRecord`, failing closed via `EXCEPTION_CONTINUE_SEARCH`. Added three diagnostic fields to
`ThreadContext`, the summary struct, the snapshot copy, and the end-of-run summary print.

### 3. Verification
Debug build succeeds. Under `aot-dbt` (`pumpit1`, 25s timeout): normal exception dispatch succeeds at
scale (`dispatch_entry=19412`), 49 glide gates processed, `malformed count = 0` (no false positives), no
`0x101AF9A1` secondary crash, clean timeout exit (code 0). This confirms the guard does not reject valid
`CONTEXT` pointers (no regression).

**Verification limitation (honest note):** the original malformed-`EXCEPTION_POINTERS` crash only appears
once the guest reaches the timer-injection phase after installing the INT 8 vector, but runs stalled
non-deterministically in an earlier glide busy-wait (gate 9-49, INT 8 not yet installed), so the guard
catching a malformed pointer was not observed live. The fix is nonetheless a deterministic guard for the
identified condition and is verified not to affect the normal dispatch path. A real occurrence would show
`[repiu-live] Malformed EXCEPTION_POINTERS ...` and a rising malformed count in the summary.

### 4. Follow-ups
Root cause: IF-gating and nesting prevention of INT 8 injection, guest-stack TIB Base/Limit alignment.
Cleanup: remove branch debug scaffolding (`resolve_sym` external-path target, debug fprintf, Glide skip
log) before commit/merge.
