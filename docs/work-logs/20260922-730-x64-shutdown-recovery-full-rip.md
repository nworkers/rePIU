# Task 730 작업 로그 — x64 종료 회수 판정에 전체 RIP 사용

설계: [20260922-730](../design/20260922-730-x64-shutdown-recovery-full-rip.md)
작업 지시: [20260922-730](../work-orders/20260922-730-x64-shutdown-recovery-full-rip.md)

## 결과

회수 판정을 순수 함수 `DecideShutdownRecovery`
(`include/repiu/engine/shutdown_recovery_policy.h`)로 추출했습니다. x64 Linux에서는
`ReadHostInstructionPointer(host_context)`로 전체 RIP를 읽고, **상위 32비트가 0이면서 하위 32비트가
guest 이미지나 AOT cache 안일 때만** 회수합니다. 하위 32비트는 guest 범위인데 전체 주소가 host
주소인 경우, 즉 이전 코드였다면 잘못 회수했을 경우를 `kAliasedHostAddress`로 거절하고 횟수를
셉니다. Win32 x86과 Linux i386은 native 포인터가 32비트라 판정이 이전과 같습니다.

`[repiu-shutdown]` 줄에 `host_ip=`(마지막으로 본 전체 주소), `decision=`, `aliased=`를
추가했습니다. 시그널 핸들러 안에서는 정수만 갱신하고, 출력은 요청 스레드가 합니다.

## 검증

- Win32 x86 Debug 전체 빌드 성공. `repiu_aot_probe --shutdown-recovery-policy` 통과
  (`wide_pointer=false`라 alias 경우는 이 host에서 건너뜀). Win32 core probe 29/29.
- WSL Linux x64 Debug 빌드 성공. core probe **31/31**, 새 `shutdown_recovery_policy` 그룹이
  `wide_pointer=true`로 alias 거절·4 GiB 경계·주소 미확보 거절을 모두 통과했습니다. alias 경우를
  실제로 검사할 수 있는 곳은 x64 host뿐이므로 core probe에도 등록했습니다.
- WSL에서 Task 729와 같은 설정으로 OFF·ON 번갈아 5회씩, 순차 10회 관찰:

| | 수정 전 (Task 729) | 수정 후 |
|---|---:|---:|
| 실행 | 12 | 10 |
| teardown segfault | **2** | **0** |
| `aliased>0` 실행 | (측정 불가) | 0 |
| 정상 회수(`decision=recover`) | — | 1 (OFF run 2) |

- OFF run 2는 guest 코드(`0x01030E9C`, 상위 32비트 0)에서 **정상 회수**되어 clean teardown 전
  단계(`glide-close`부터 `done`까지)와 summary를 남겼습니다. 이번 수정이 정당한 회수를 막지 않는다는
  확인입니다. 나머지 9회는 호스트 라이브러리(`0x7f....E4F`)에서 `outside-guest-code`로 40회 모두
  거절됐고, 이전처럼 immediate-exit 경로로 summary를 남겼습니다.

## 해석 — 결함은 닫았지만 인과는 아직 확정되지 않았다

* **확인됨:** 이전 판정은 x64에서 하위 32비트만 봤으므로, 하위 절반이 guest 범위에 드는 host
  주소를 회수할 수 있었습니다. 이 경로는 결정적 probe로 재현되며 수정으로 닫혔습니다.
* **확인되지 않음:** 관찰된 teardown segfault가 이 경로 때문이었다는 것. 수정 후 10회에서 크래시는
  0이었지만, 원래 발생률(약 1/6)이 그대로여도 10회 연속 무사할 확률이 약 16%입니다. 또한 10회
  모두 `aliased=0`이라, alias 상황 자체가 이 10회에서는 한 번도 일어나지 않았습니다.
* 흥미로운 관찰: 마지막으로 본 host 주소의 하위 12비트가 9회 모두 `0xE4F`였습니다. ASLR은
  라이브러리 base만 옮기므로 **같은 호스트 함수의 같은 명령**에서 guest thread가 자주 멈춰 있다는
  뜻입니다. 이 주소의 하위 32비트가 guest 범위(32비트 공간의 약 3.3%)에 들어가는 실행에서만 alias가
  생기므로, alias가 원인이라면 발생률은 1/6보다 낮을 것으로 예상됩니다. 발생률과 이 추정의 차이는
  아직 설명되지 않았습니다.

결론의 확정에는 더 많은 실행(30회 이상이면 원래 발생률이 유지될 때 모두 무사할 확률이 1% 미만)이
필요합니다.

## 조사만 한 것 — 같은 32비트 판정을 쓰는 다른 경로

`execution_trampoline.cpp`에서 `win32_context->Eip`(x64에서는 하위 32비트)로 guest/cache 여부를
판정하는 곳이 더 있습니다. fault를 처리하며 **동작을 바꾸는** 곳은 4851행 부근(guest/cache가 아니면
처리 거절)과 6327~6357행 부근(cache 주소면 복구 경로 선택)입니다. host 라이브러리 안의 fault가
하위 절반 alias를 일으키면 같은 종류의 오판이 가능하지만, 이 작업에서는 고치지 않았습니다.

---

# English

# Task 730 work log — using the full RIP in the x64 shutdown recovery decision

Design: [20260922-730](../design/20260922-730-x64-shutdown-recovery-full-rip.md)
Work order: [20260922-730](../work-orders/20260922-730-x64-shutdown-recovery-full-rip.md)

## Result

The recovery decision is extracted into the pure function `DecideShutdownRecovery`
(`include/repiu/engine/shutdown_recovery_policy.h`). On x64 Linux it reads the full RIP through
`ReadHostInstructionPointer(host_context)` and recovers **only when the upper 32 bits are zero and
the low 32 bits are in the guest image or AOT cache**. A low half in guest range whose full address is
a host address -- the case the old code would have recovered wrongly -- is refused as
`kAliasedHostAddress` and counted. Win32 x86 and Linux i386 have 32-bit native pointers and keep the
previous decision.

The `[repiu-shutdown]` line gains `host_ip=` (the last full address seen), `decision=` and
`aliased=`. The signal handler only updates integers; the requesting thread prints.

## Verification

- Full Win32 x86 Debug build succeeded. `repiu_aot_probe --shutdown-recovery-policy` passed
  (`wide_pointer=false`, so this host skips the alias case). Win32 core probe 29/29.
- WSL Linux x64 Debug build succeeded. Core probe **31/31**; the new `shutdown_recovery_policy`
  group ran with `wide_pointer=true` and passed alias refusal, the 4 GiB boundary and refusal
  without an address. Only an x64 host can exercise the alias case, which is why the probe is also
  registered in the core probe.
- Ten sequential WSL observations under Task 729's settings, OFF and ON alternating five times each:
  teardown segfaults went from **2 of 12** before the fix to **0 of 10** after; no run had
  `aliased>0`; one run (OFF run 2) recovered legitimately with `decision=recover`.
- OFF run 2 was **recovered legitimately** from guest code (`0x01030E9C`, upper bits zero) and left
  the full clean teardown (`glide-close` through `done`) and its summary, confirming that the fix
  does not block a legitimate recovery. The other nine refused all 40 attempts as
  `outside-guest-code` inside a host library (`0x7f....E4F`) and reported through the immediate-exit
  path as before.

## Interpretation — the defect is closed, but causation is not established

* **Confirmed:** the old decision read only the low 32 bits on x64, so it could recover a host
  address whose low half fell in guest range. That path reproduces in a deterministic probe and the
  fix closes it.
* **Not confirmed:** that the observed teardown segfault came from this path. The ten runs after the
  fix had zero crashes, but at the original rate of about 1 in 6 the chance of ten clean runs is
  about 16%. All ten also reported `aliased=0`, so the alias situation itself never arose in them.
* Notably, the low 12 bits of the last host address were `0xE4F` in all nine refusing runs. ASLR
  moves only the library base, so the guest thread is usually parked at **the same instruction of the
  same host function**. An alias arises only in runs where that address's low 32 bits land in guest
  range (about 3.3% of the 32-bit space), so if aliasing were the cause the rate would be expected
  below 1 in 6. That discrepancy is not yet explained.

Settling it needs more runs: with 30 or more, the chance of all passing at the original rate falls
below 1%.

## Surveyed only — other paths using the same 32-bit decision

`execution_trampoline.cpp` has further decisions on `win32_context->Eip`, the low 32 bits on x64.
The ones that **change behavior** while handling a fault are near line 4851 (refusing to handle
outside guest/cache) and lines 6327-6357 (choosing the recovery path for a cache address). A fault
inside a host library whose low half aliases guest code could be misjudged the same way; they are not
changed here.
