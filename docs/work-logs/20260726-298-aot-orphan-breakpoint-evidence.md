# 20260726-298 작업 로그: AOT orphan breakpoint 증거 캡처 / Work log

설계: [docs/design/20260726-298-aot-orphan-breakpoint-evidence.md](../design/20260726-298-aot-orphan-breakpoint-evidence.md)

작업 지시: [docs/work-orders/20260726-298-aot-orphan-breakpoint-evidence.md](../work-orders/20260726-298-aot-orphan-breakpoint-evidence.md)

## 한국어

### 결과

처리되지 않은 Win32 `EXCEPTION_BREAKPOINT`의 진입 상태를 기존 처리기보다 먼저 고정
크기 구조에 캡처하고, 어떤 처리기도 소비하지 못해 최종 fail-closed 경로에 도달한
경우에만 실행 결과로 보존하도록 구현했습니다. breakpoint를 자동으로 재개하거나
guest EIP/ESP를 바꾸는 동작은 추가하지 않았습니다.

hot path 교란을 줄이기 위해 진입 시에는 O(1) 원시 값만 보존하고, address-map 검색과
Win32 메모리 조회는 실제 unhandled 경로의 한 건에 대해서만 수행합니다.

### 구현

| 파일 | 변경 |
|---|---|
| `include/repiu/platform/win32/execution_trampoline.h` | 공개 `Win32UnhandledBreakpointEvidence` 값 구조와 상태 bit 정의 추가 |
| `src/platform/win32/exception/breakpoint_evidence_win32.h/.cpp` | O(1) raw entry 캡처와 unhandled-only AOT 매핑·provenance, byte/stack 보강 및 최종 커밋 구현 |
| `src/platform/win32/execution/execution_trampoline.cpp` | 포인터 검증 뒤 캡처하고 최종 `CaptureException` 직전에만 커밋 |
| `src/platform/win32/execution/thread_context.h` | 실행 중 증거 저장 추가 |
| `src/host/win32/main.cpp` | 최종 loader summary에 구조화된 증거 출력 |
| `CMakeLists.txt` | 새 Win32 예외 계측 source 등록 |

상태 bit는 `aot_reentry_pending`, single-step trace, native fast path, native linear
span, native region 순서입니다. 원시 exception address와 entry EIP 각각에 대해 exact와
`address - 1` 후보를 독립적으로 기록하므로 다음 로그에서 실제 cache trap 위치와
Windows breakpoint 보고 위치를 구분할 수 있습니다. 진입/최종 AOT return dispatch
count와 마지막 source/target도 함께 남겨 기존 반환 처리기가 해당 이벤트를 소비했는지
직접 비교합니다.

### 안전성

- 계측 파일은 고정 배열과 값 타입만 사용합니다.
- heap allocation, 문자열 생성, 파일 I/O, `SetThreadContext`, 예외 재개 반환을 사용하지
  않습니다.
- 메모리 창과 stack top은 `VirtualQuery`/`ReadProcessMemory`로 읽고 유효 길이와 mask를
  함께 기록합니다.
- 기존 handler 순서와 `EXCEPTION_CONTINUE_*` 결과를 변경하지 않았습니다.
- 사용자 로그 `repiu_log.txt`는 수정하거나 commit 대상에 포함하지 않았습니다.

### 검증

1. `cmake --build build/win32_x86_debug --config Debug --target repiu_loader_win32`
   성공. 결과: `build/win32_x86_debug/Debug/repiu_loader_win32.exe`.
2. 첫 compile에서 Win32 `min/max` 및 `exception_code` macro 충돌을 확인했고,
   `NOMINMAX`와 중립 필드명 `code`로 수정한 뒤 재빌드에 성공했습니다.
3. 계측 전용 파일에 allocation/string/file/recovery API가 없음을 `rg`로 정적
   확인했습니다.
4. `git diff --check`가 통과했습니다.
5. 실제 증거 값은 다음 장기 재현 로그가 필요하므로 아직 미확정입니다.

기존 C4819 코드 페이지 경고는 세 파일과 spdlog header에서 계속 출력됐으며 이번
변경으로 새로 발생한 경고는 아닙니다.

---

## English

### Result

Unhandled Win32 `EXCEPTION_BREAKPOINT` entry state is now captured into a
fixed-size value before normal handlers run and preserved in the execution
result only if no handler consumes the event and dispatch reaches the final
fail-closed path. The change does not resume a breakpoint or modify guest EIP
or ESP.

Entry capture retains O(1) scalar state only. Address-map searches and Win32
memory reads run only for the single event that remains unhandled, avoiding
instrumentation overhead on the high-rate handled AOT-breakpoint path.

### Implementation

`Win32UnhandledBreakpointEvidence` carries raw/entry/final addresses and
register state, execution-state bits, entry/final AOT return-dispatch state,
exact and minus-one AOT mappings and
provenance, two byte windows, and the top four stack dwords. A dedicated
`breakpoint_evidence_win32.h/.cpp` module owns capture and commit, while the
execution trampoline only orchestrates its entry and final calls. The loader
prints the packet in the final exception summary.

The state bits represent AOT reentry pending, single-step trace, native fast
path, native linear span, and native region. Exact and `address - 1` candidates
are captured independently for both the raw exception address and entry EIP.

### Safety

The module uses fixed arrays and value types only. It performs no heap
allocation, string creation, file I/O, context mutation, or exception-resume
decision. `VirtualQuery` and `ReadProcessMemory` reads carry valid lengths and
masks. Existing handler order and return values are unchanged. The user-owned
`repiu_log.txt` remains untouched and excluded from the commit.

### Verification

The Win32 x86 Debug `repiu_loader_win32` target builds successfully. Initial
Windows `min/max` and `exception_code` macro conflicts were corrected with
`NOMINMAX` and the neutral field name `code`; the rebuild passed. Static search
found no allocation, string, file, recovery, or continue-execution APIs in the
instrumentation module, and `git diff --check` passed. Runtime evidence values
remain pending the next long-run reproduction. Existing C4819 warnings are
unrelated to this change.
