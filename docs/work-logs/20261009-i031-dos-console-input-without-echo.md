# issue #31 (Task 764) 작업 로그 — INT 21h AH=07h/08h 에코 없는 콘솔 입력

> **경위(2026-10-09, issue #31):** 이 작업은 2026-10-01 Task 764(`ef3ffc4`)로 끝났지만, 그 커밋이 있던
> 브랜치 `docs/763-close-resolved-frontier-items`가 머지되지 않은 채 사라져 main에 들어가지 않았습니다.
> 2026-10-09 같은 변경을 main 위에 다시 적용했습니다(같은 브랜치의 Task 765는 #8). 본문은 Task 764
> 당시의 기록입니다.


설계: [20261009-i031](../design/20261009-i031-dos-console-input-without-echo.md)
작업 지시: [20261009-i031](../work-orders/20261009-i031-dos-console-input-without-echo.md)

## 요약

게임의 fatal 처리기가 `mov ah,8; int 21h`(Watcom `getch`)로 키를 기다리다 `unsupported DOS INT 21h AH=0x8`로
죽던 공백(Task 751)을 메웠습니다. `AH=07h`·`08h`가 BIOS 키보드 버퍼를 읽어 `AL`에 문자를 돌려주고, 확장 키는
scan code를 다음 호출에 돌려주며, 버퍼가 비면 1 ms 잔 뒤 `EIP`를 그대로 두어 게스트가 다시 묻게 했습니다.
pumpipx3의 fatal 경로가 이제 키를 받고 `exit(-1)`로 정상 종료합니다.

## 구현

* `ThreadContext`: `dos_console_pending_scan_code`(+`_valid`), `dos_console_input_count`,
  `dos_console_input_wait_count`.
* `dos_int21_services.cpp`: `HandleDosConsoleInputWithoutEcho`, `HandleDosInterrupt21`의 `case 0x07`·`0x08`.
  빈 버퍼는 `RecordHandledDosInterrupt`를 세지 않고 `EIP` 불변으로 `true`.
* `instruction_emulation.cpp`: `HandleTracedDosInterrupt21`의 위임 분기에 `0x07`·`0x08`.
* 최종 보고에 `DOS console input chars/waits` 한 줄(`execution_trampoline.h`의 attempt 필드,
  `live_telemetry_snapshot.cpp` 복사, `loader/main.cpp` 출력).
* probe `dos_console_input`(`src/tools/aot_probe/dos_console_input_probe.{h,cpp}`): core probe 전 호스트,
  aot probe 전체와 `--dos-console-input`. 64비트 호스트에서는 traced dispatcher 부분을 건너뛰고
  `traced_skipped=true`로 표시합니다 — traced dispatcher는 `EIP`의 바이트를 읽는데 64비트 로컬 버퍼 주소는
  32비트 `EIP`에 담기지 않습니다(처음 버전은 이 때문에 Linux x64 probe가 segfault했습니다).

## 검증

| 검증 | 결과 |
|---|---|
| Win32 x86 Debug `repiu_core_probe` | `dos_console_input_all=true`(5항목), `core_probe_all=true` |
| Win32 x86 Debug `repiu_aot_probe --dos-console-input` | exit 0 |
| Win32 x86 Release `repiu_core_probe` | `dos_console_input_all=true`, `core_probe_all=true` |
| Linux i386(WSL) `repiu_core_probe` | `dos_console_input_all=true`, `core_probe_all=true` |
| Linux x64(WSL, Release) `repiu_core_probe` | `dos_console_input_all=true`(`traced_skipped=true`), `core_probe_all=true` |
| pumpipx3 Win32 Release, `REPIU_TIMER_RETURN_PAD=0`(fatal 경로 유도), 키 없음, 40초 | 이전처럼 죽지 않고 `mov ah,8; int 21h`(`0x040FB507`)에서 예산 만료까지 기다림. 입력 스크립트의 키 이름 오류(`Return`, 0 steps)로 키가 가지 않았음 |
| 같은 조건, Enter를 6초부터 4초마다 | **`DOS console input chars/waits: 1/1499`**, `DOS termination captured: true`(`AX=0x4C00`), 9초에 정상 종료. 이전(762 pad-off 로그)은 `unsupported DOS INT 21h AH=0x8`로 폴트 종료 |

로그: `build/task764/`(저장소 밖), 실행 스크립트 `build/task764.sh`, 키 스크립트 `build/task764/getch_keys.txt`.

## 재적용 검증 (2026-10-09, issue #31)

`ef3ffc4`를 main(v0.0.210) 위에 다시 적용했습니다. 충돌은 세 곳이었습니다.
* `aot_probe/main.cpp`: 그 사이 추가된 probe 진입점 뒤에 `--dos-console-input`을 두었습니다.
* `current-execution-frontier.md`: 충돌 부분은 버리기로 한 Task 763의 열린 항목 표뿐이라 main을 따랐습니다.
* `linux-port-frontier.md`: main을 따르고 AH=08h 줄만 해결로 바꿨습니다.

#18(v0.0.206)의 INT 21h host 디스패치는 AH=2Ch·42h만 받으므로, AH=07h/08h는 그대로 VEH 경로를
지나며 "EIP를 `int 21h`에 둬 다시 묻는" 대기가 유효합니다.

| 검증 | 결과 |
|---|---|
| Win32 Release·Linux x64 Release 빌드 | 오류 0 |
| `repiu_aot_probe --dos-console-input`(Win32) | ordinary·extended·waits·direct·traced 모두 true, exit 0 |
| Win32 aot_probe 전체 체인 | exit 0, 546줄(이 probe 7줄 추가). 위의 "전체 aot probe exit 1"은 #22의 삭제와 그 기대값 수정으로 이미 해소됐습니다 |
| core probe | Linux x64 모두 통과(traced는 64비트 호스트라 skip). Win32는 `stack_bridge`만 실패 — #8 수정 전 main에서 갈라진 브랜치이기 때문 |

게임 실행은 하지 않았습니다. 이 경로는 fatal 처리기에서만 닿습니다(당시 pumpipx3에서 return pad를 꺼서 유도).

## 확인하지 않은 것, 남은 것

* **HLE 안에서 자는 게스트 스레드는 예산 만료 teardown에 "복구 가능한 코드 밖"으로 보입니다.** 키 없이 40초를
  기다린 실행의 종료는 `guest thread was not in recoverable code`(eip는 ntdll)였고 종료 코드는 0이었습니다.
  Glide 게이트의 대기와 같은 모양이므로 이 작업에서는 두었습니다.
* Linux x64에서 게임이 이 경로에 닿는 실행은 하지 않았습니다(core probe만).
* **전체 aot probe(`repiu_aot_probe roms/pumpit8/PIU/PIU.EXE`)가 `dbt_indirect_dispatch_call_layout=false`·
  `placement=false`로 exit 1입니다.** 이 작업이 손대지 않은 emitter 배치 검사이고, 전체 통과가 마지막으로
  기록된 것은 Task 482(2026-08-22)입니다. 기존 회귀로 보고 Win32 frontier의 열린 항목에 더했습니다.
* 부수 관찰(pumpipx3 "entry point" fatal의 기전): pad를 끈 실행에서 주입 85회 중 44회가 `sti` 지점에서
  전달됐고 frame 주소가 0x40씩 내려가는 쌍이 반복되며 종료 시 nesting depth가 4였습니다. 38번째 get-proc은
  게이트에 닿지 않았습니다. ISR 중첩이 조회 루프의 상태를 망가뜨린다는 가설까지이며, 게스트 수준 추적은 하지
  않았습니다.

---

# English

> **History (2026-10-09, issue #31):** this was finished on 2026-10-01 as Task 764 (`ef3ffc4`), but the
> branch holding it, `docs/763-close-resolved-frontier-items`, disappeared unmerged. The same change was
> reapplied on main on 2026-10-09 (that branch's Task 765 became #8). The body is the Task 764 record.

# Task 764 work log — INT 21h AH=07h/08h console input without echo

Design: [20261009-i031](../design/20261009-i031-dos-console-input-without-echo.md)
Work order: [20261009-i031](../work-orders/20261009-i031-dos-console-input-without-echo.md)

## Summary

Closed the gap from Task 751: the game's fatal handler waited for a key with `mov ah,8; int 21h`
(Watcom's `getch`) and died with `unsupported DOS INT 21h AH=0x8`. `AH=07h` and `08h` now read the
BIOS keyboard buffer into `AL`, an extended key returns its scan code on the next call, and an empty
buffer sleeps 1 ms and leaves `EIP` in place so the guest asks again. pumpipx3's fatal path now takes
the key and leaves through `exit(-1)`.

## Implementation

* `ThreadContext`: `dos_console_pending_scan_code` (+`_valid`), `dos_console_input_count`,
  `dos_console_input_wait_count`.
* `dos_int21_services.cpp`: `HandleDosConsoleInputWithoutEcho`, `case 0x07` and `0x08` of
  `HandleDosInterrupt21`. An empty buffer returns `true` with `EIP` unchanged and without counting a
  handled interrupt.
* `instruction_emulation.cpp`: `0x07` and `0x08` on the delegating branch of `HandleTracedDosInterrupt21`.
* One final-report line, `DOS console input chars/waits` (attempt fields in `execution_trampoline.h`,
  the copy in `live_telemetry_snapshot.cpp`, the output in `loader/main.cpp`).
* Probe `dos_console_input` (`src/tools/aot_probe/dos_console_input_probe.{h,cpp}`): in the core probe
  on every host, in the full aot probe and as `--dos-console-input`. On 64-bit hosts the traced
  dispatcher part is skipped and reported as `traced_skipped=true`: that dispatcher reads the bytes at
  `EIP`, and a 64-bit local buffer's address does not fit a 32-bit `EIP` (the first version segfaulted
  the Linux x64 probe for that reason).

## Verification

| Check | Result |
|---|---|
| Win32 x86 Debug `repiu_core_probe` | `dos_console_input_all=true` (5 items), `core_probe_all=true` |
| Win32 x86 Debug `repiu_aot_probe --dos-console-input` | exit 0 |
| Win32 x86 Release `repiu_core_probe` | `dos_console_input_all=true`, `core_probe_all=true` |
| Linux i386 (WSL) `repiu_core_probe` | `dos_console_input_all=true`, `core_probe_all=true` |
| Linux x64 (WSL, Release) `repiu_core_probe` | `dos_console_input_all=true` (`traced_skipped=true`), `core_probe_all=true` |
| pumpipx3 Win32 Release, `REPIU_TIMER_RETURN_PAD=0` (to walk the fatal path), no key, 40 s | no longer dies; waits at `mov ah,8; int 21h` (`0x040FB507`) until the budget ends. A wrong key name in the script (`Return`, 0 steps) meant no key was sent |
| The same with Enter from 6 s every 4 s | **`DOS console input chars/waits: 1/1499`**, `DOS termination captured: true` (`AX=0x4C00`), clean exit at 9 s. Before (762's pad-off log): a fault ending on `unsupported DOS INT 21h AH=0x8` |

Logs in `build/task764/` (outside the repository); run script `build/task764.sh`, key script
`build/task764/getch_keys.txt`.

## Reapplied verification (2026-10-09, issue #31)

`ef3ffc4` was reapplied on main (v0.0.210) with three conflicts: `aot_probe/main.cpp` (the
`--dos-console-input` entry now follows the probe entries added since), `current-execution-frontier.md`
(the conflict was only Task 763's open-items table, which was dropped, so main's text stands) and
`linux-port-frontier.md` (main's text, with the AH=08h line marked resolved). #18's (v0.0.206) INT 21h
host dispatch takes only AH=2Ch and 42h, so AH=07h/08h still go through VEH and the wait that leaves EIP
on the `int 21h` still holds. Win32 and Linux x64 Release builds have no errors; `repiu_aot_probe
--dos-console-input` on Win32 passes all five checks; the full Win32 aot_probe chain exits 0 with 546
lines (seven from this probe) — the "full aot probe exits 1" noted above has since been resolved by #22's
deletions and its expectation fix; the Linux x64 core probe passes (traced skipped on a 64-bit host), and
the Win32 one fails only `stack_bridge`, because this branch predates #8's fix. No game run: the path is
reached only from the fatal handler (induced at the time by turning pumpipx3's return pad off).

## Not checked, and left

* **A guest thread sleeping inside the HLE looks like "not in recoverable code" to the budget-expiry
  teardown.** The no-key 40 s run ended with `guest thread was not in recoverable code` (eip in ntdll)
  and exit code 0. It has the same shape as a Glide gate's wait, so it is left as is here.
* No Linux x64 game run reaches this path in this task (core probe only).
* **The full aot probe (`repiu_aot_probe roms/pumpit8/PIU/PIU.EXE`) exits 1 with
  `dbt_indirect_dispatch_call_layout=false` and `placement=false`.** That is an emitter-layout check this
  task did not touch, and the last recorded full pass is Task 482 (2026-08-22). Treated as a
  pre-existing regression and added to the Win32 frontier's open items.
* A side observation on the mechanism of pumpipx3's "entry point" fatal: in the pad-off run 44 of 85
  injections were delivered at a `sti`, frame addresses repeated in pairs 0x40 apart, and the nesting
  depth at the end was 4. The 38th get-proc never reached the gate. That supports the hypothesis that
  ISR nesting corrupts the lookup loop's state; no guest-level trace was taken.
