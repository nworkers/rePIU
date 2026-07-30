# 20260730-366 Timer tick 전달과 프레임 pacing 작업 지시 / Work order

* 설계: [20260730-366-timer-tick-delivery-and-frame-pacing.md](../design/20260730-366-timer-tick-delivery-and-frame-pacing.md)
* 근거: [Task 365 작업 로그](../work-logs/20260730-365-glide-setter-state-elision.md)
* 범위: 1단계 상시 counter + 2단계 opt-in backlog 전달. 기본 동작은 1단계에서 불변.

## 한국어

### 1. 구현 항목

| # | 파일 | 내용 |
|---|---|---|
| 1 | `include/repiu/platform/win32/timer_tick_delivery.h` | counter 자료형, backlog 정책, 기록/스냅샷 API |
| 2 | `src/platform/win32/telemetry/timer_tick_delivery.cpp` | 위 구현 (`REPIU_TIMER_TICK_BACKLOG`) |
| 3 | `src/platform/win32/execution/thread_context.h` | counter 보유 |
| 4 | `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | poll loop에서 due 기록, 스냅샷 수집 |
| 5 | `src/platform/win32/execution/execution_trampoline.cpp` | `InjectPendingInterrupts`에서 injected/coalesced/deferred 기록과 backlog 소진 |
| 6 | `include/repiu/platform/win32/execution_trampoline.h` | attempt 스냅샷 필드 |
| 7 | `src/host/win32/main.cpp` | 종료 요약 |
| 8 | `src/tools/aot_probe/timer_tick_delivery_probe.{h,cpp}` | 단위 probe |
| 9 | `src/tools/aot_probe/main.cpp`, `CMakeLists.txt` | probe 등록과 빌드 |
| 10 | `scripts/task366_timer_tick_delivery.ps1` | 3회 OFF/ON A/B와 gate M1~M6 |

### 2. 필수 제약

* 1단계 counter는 상시 ON이지만 **동작을 바꾸지 않습니다.** 기존 bool 전달 의미가
  그대로 유지되어야 하고, counter만 추가됩니다.
* 2단계 backlog는 기본 OFF(`REPIU_TIMER_TICK_BACKLOG=1`로 활성). 활성 시에도
  **주입 1회의 의미는 불변**입니다. EFLAGS/CS/EIP push, IF/TF 처리, IF=0·비-guest EIP
  미룸 조건을 그대로 둡니다. 바뀌는 것은 주입 **횟수**뿐입니다.
* burst 주입 금지. safe point당 최대 1회만 주입하고 backlog는 다음 기회로 넘깁니다.
* backlog 상한 `kMax`(64)를 넘는 due는 `dropped_total`로 세고 버립니다. 미래 tick을
  앞당기지 않습니다.
* hot path에서 allocation, 문자열 생성, clock read를 하지 않습니다.
* `PitIrqSchedule`과 `PitChannel0`의 의미는 바꾸지 않습니다.

### 3. 검증 절차

1. `scripts/build_win32_x86.bat` (Debug) 통과
2. `scripts/build_win32_x86_release.bat` (Release) 통과
3. `repiu_aot_probe.exe` exit 0 — 두 구성, 신규 probe 포함
4. `scripts/task366_timer_tick_delivery.ps1 -Runs 3 -DurationSeconds 60`
   * 설계 §7의 M1~M6을 script가 검사하고 위반 시 throw
   * **M1(`due == injected + coalesced + 잔여`)은 분해 경계의 근거이므로 필수**
5. 설계 §6의 T1~T4 중 하나를 판정해 작업 로그에 기록

### 4. 완료 조건

* 두 구성 빌드와 probe 통과
* M1~M4 통과, M5 기록
* T1~T4 판정과 다음 작업 순서가 문서에 기록됨
* 1단계가 기본 동작을 바꾸지 않았음이 diff와 A/B로 확인됨

---

## English

### Scope

Stage one adds always-on counters that change no behaviour, so the existing
boolean delivery keeps its exact meaning and only the accounting is new. Stage two
adds an opt-in bounded backlog behind `REPIU_TIMER_TICK_BACKLOG=1` in which a
single injection means exactly what it means today — same pushed frame, same IF
and TF handling, same deferral for IF=0 and non-guest EIP — and only the number of
injections changes. Injection stays at most one per safe point so a backlog never
bursts into the guest stack, and owed ticks beyond a cap of 64 are counted as
dropped rather than delivered early.

### Verification

Both builds and the probe suite must pass, then
`scripts/task366_timer_tick_delivery.ps1 -Runs 3 -DurationSeconds 60` must
satisfy M1 through M6, throwing on violation. M1, the partition identity
`due == injected + coalesced + remainder`, is mandatory because it is what makes
the decomposition boundaries trustworthy. The run then decides one of T1 through
T4, recorded in the work log with the resulting next order, and stage one must be
shown by diff and A/B to have left default behaviour unchanged.
