# 20260726-299 작업 로그: 타이머 선점 VEH rendezvous / Work log

설계: [docs/design/20260726-299-timer-preemption-veh-rendezvous.md](../design/20260726-299-timer-preemption-veh-rendezvous.md)

작업 지시: [docs/work-orders/20260726-299-timer-preemption-veh-rendezvous.md](../work-orders/20260726-299-timer-preemption-veh-rendezvous.md)

## 한국어

### 결과

poll thread가 guest 스택에 12바이트 IRET frame을 직접 쓰고 ESP/EIP/CS를 바꾸던
선점 주입을 제거했습니다. poll thread는 pending을 설정하고 TF만 arm하며, 다음
`EXCEPTION_SINGLE_STEP`을 받은 guest thread의 VEH가 wakeup용 TF를 제거한 뒤 기존
`InjectPendingInterrupts`를 호출합니다.

원본 ISR `0x03042EAE`와 원본 `IRETD`, IF 게이트, pending coalescing은 변경하지
않았습니다.

### 근인 증거

사용자 로그의 1차 실패는 `RET 0x030D8BB2`가 `0x43F00000`(`480.0f`)을 반환주소로
꺼낸 것입니다. 정상 반환주소 `0x030F2739`는 `[ESP+12]`에 남아 있었습니다.
정적 disassembly로 다음 ABI가 균형임을 확인했습니다.

- `0x030F2734: call 0x030D8B84`, 정상 복귀 `0x030F2739`
- wrapper: `call [edx+0x2C4]; add esp,8; ret`
- 실제 대상 `0x03062560`: `ret 8`

따라서 12바이트 차이는 호출 규약 오류가 아니라 한 개의 INT 8 IRET frame과 같은
크기입니다. PDB로 확인한 host `HandleTracedDosInterrupt21` AV는 이미 비정상인
`EIP=0x43F00000`을 opcode probe가 읽은 2차 실패입니다.

### 구현

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/thread_context.h` | atomic `timer_interrupt_wakeup_armed` 추가 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 직접 frame/ESP/EIP/CS 변경 제거, EIP/IF/TF 검증 후 pending+armed+TF 설정 |
| `src/platform/win32/execution/execution_trampoline.cpp` | armed single-step을 guest thread에서 소비하고 공통 주입기로 전달 |
| `docs/analysis/interrupts-and-port-io.md` | 12바이트 frame 누수와 새 주입 정책 누적 |
| `docs/analysis/current-execution-frontier.md` | 1차/2차 실패와 다음 frontier 갱신 |

`SetThreadContext`가 실패하면 armed만 해제하고 pending은 유지합니다. poll 시점에 TF가
이미 설정돼 있으면 기존 single-step 소유권을 침범하지 않습니다. wakeup보다 다른 예외가
먼저 오면 wakeup용 TF와 armed만 제거하고 원래 예외 dispatch를 계속합니다.

### 검증

#### 빌드

기존 `build/win32_x86_debug`의 CMake cache가 설치되지 않은
`Visual Studio 18 2026` generator를 가리키고 있어 해당 사용자 상태를 수정하지
않았습니다. 저장소에 이미 받은 SDL3/spdlog source를 재사용해
`build/task299_vs2022_debug`를 VS2022 Win32로 구성했고
`repiu_loader_win32` Debug 빌드에 성공했습니다.

기존 C4819 코드페이지 경고만 출력됐고 새 compile/link 오류는 없었습니다.

#### 45초 직접 계측

`build/task299-veh-rendezvous-smoke/`에서 다음 조건으로 한 번 실행했습니다.

- backend: `aot-dbt`
- timeout: 45,000 ms
- `REPIU_TIMER_INJECT_LOG=1`
- task297 EEPROM fixture의 격리 복사본

결과:

| 항목 | 값 |
|---|---:|
| 종료 | 정상 timeout, exit 0 |
| exception caught / malformed | false / 0 |
| TF arm / VEH wakeup | 3 / 3 |
| poll-thread 직접 preemptive frame write | 0 |
| 전체 INT 8 frame 주입 / chain HLE | 334 / 333 |
| AOT non-guest return fallback | 0 |
| AOT last fallback address | `0x00000000` |
| diagnostic progress | 28,128 |
| Glide gate | 2,212 / 2,212 |
| Glide window | 1 / 640x480 |

세 rendezvous는 모두 `Armed → VEH wakeup → Injected` 순서로 1:1 대응했습니다.
이전 실패 시점 약 29초와 progress 13,737을 넘어 45초와 progress 28,128까지
진행했으며 `RET 0x030D8BB2 -> 0x43F00000` 및 ESP-12 실패는 재현되지 않았습니다.

EEPROM SHA-256은 실행 전후 모두
`A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`으로
일치했습니다.

### 검증 한계

45초 단일 실행은 무제한 장기 안정성을 증명하지 않습니다. 다만 기존 실패 시점을
넘겼고, poll-thread 직접 frame write 제거, rendezvous 1:1 대응, non-guest return
fallback 0, exception 0을 함께 확인해 대상 `ESP-12` 결함은 이 검증 범위에서
제거됐습니다.

---

## English

### Result

The poll thread no longer writes a 12-byte IRET frame or modifies guest
ESP/EIP/CS. It sets pending and arms TF only. The guest thread consumes the
next private single-step in VEH, removes the wakeup-only TF, and calls the
existing `InjectPendingInterrupts`. The original ISR, original `IRETD`, IF
gate, and pending coalescing remain unchanged.

The user log showed the primary failure as `RET 0x030D8BB2` reading
`0x43F00000` (`480.0f`) while the correct `0x030F2739` return remained at
`[ESP+12]`. Static disassembly confirms balanced caller, wrapper, and
`ret 8` callee ABIs, identifying the displacement as one INT 8 IRET frame.
The PDB-resolved `HandleTracedDosInterrupt21` AV was secondary.

### Verification

Because the existing build cache points at an unavailable Visual Studio 2026
generator, it was left untouched. A separate VS2022 Win32 tree reused the
already-downloaded SDL3/spdlog sources and built the Debug loader successfully.

One isolated 45-second `aot-dbt` run with timer logging exited by normal
timeout: no caught exception, malformed count zero, three TF arms matched
three VEH wakeups and injections, zero direct preemptive frame writes, 333 INT
8 chain completions, non-guest return fallback zero, progress 28,128, and
2,212/2,212 Glide gates. It passed the previous roughly 29-second failure
point without the `0x43F00000` return or ESP-12 shape. The isolated EEPROM hash
was unchanged.

A single 45-second run does not establish unlimited long-run stability, but it
directly verifies the new ownership boundary and removes the targeted failure
within and beyond the prior reproduction window.
