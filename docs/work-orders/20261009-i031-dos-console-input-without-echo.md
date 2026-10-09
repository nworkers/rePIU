# issue #31 (Task 764): INT 21h AH=07h/08h 콘솔 문자 입력 작업 지시

> **경위(2026-10-09, issue #31):** 이 작업은 2026-10-01 Task 764(`ef3ffc4`)로 끝났지만, 그 커밋이 있던
> 브랜치 `docs/763-close-resolved-frontier-items`가 머지되지 않은 채 사라져 main에 들어가지 않았습니다.
> 2026-10-09 같은 변경을 main 위에 다시 적용했습니다(같은 브랜치의 Task 765는 #8). 본문은 Task 764
> 당시의 기록입니다.


설계: [20261009-i031](../design/20261009-i031-dos-console-input-without-echo.md)

## 한국어

1. `ThreadContext`에 보류 scan code 둘(`dos_console_pending_scan_code`, `..._valid`)과 카운터 둘
   (`dos_console_input_count`, `dos_console_input_wait_count`)을 둔다.
2. `src/engine/dos/dos_int21_services.cpp`에 `HandleDosConsoleInputWithoutEcho`를 두고
   `HandleDosInterrupt21`의 `case 0x07`·`case 0x08`이 부른다. 빈 버퍼는 1 ms 대기 뒤 `EIP`를 올리지
   않고 `true`.
3. `HandleTracedDosInterrupt21`의 위임 분기에 `0x07`·`0x08`을 더한다.
4. probe `dos_console_input`을 `src/tools/aot_probe/dos_console_input_probe.{h,cpp}`에 두고 core probe와
   aot probe에 등록한다.
5. Win32 Debug로 빌드해 core probe와 aot probe를 돌리고, pumpipx3를 `REPIU_TIMER_RETURN_PAD=0`으로
   실행해 fatal 경로가 키 뒤 `exit(-1)`로 끝나는지 본다.
6. `docs/analysis/interrupts-and-port-io.md`에 서비스를 기록하고, 작업 로그를 남긴 뒤 커밋한다.

## English

> **History (2026-10-09, issue #31):** this was finished on 2026-10-01 as Task 764 (`ef3ffc4`), but the
> branch holding it, `docs/763-close-resolved-frontier-items`, disappeared unmerged. The same change was
> reapplied on main on 2026-10-09 (that branch's Task 765 became #8). The body is the Task 764 record.

1. Add to `ThreadContext` the two pending scan-code fields (`dos_console_pending_scan_code`,
   `..._valid`) and the two counters (`dos_console_input_count`, `dos_console_input_wait_count`).
2. Put `HandleDosConsoleInputWithoutEcho` in `src/engine/dos/dos_int21_services.cpp`, called from
   `case 0x07` and `case 0x08` of `HandleDosInterrupt21`. An empty buffer sleeps 1 ms and returns
   `true` without advancing `EIP`.
3. Add `0x07` and `0x08` to the delegating branch of `HandleTracedDosInterrupt21`.
4. Put the probe `dos_console_input` in `src/tools/aot_probe/dos_console_input_probe.{h,cpp}` and
   register it in the core probe and the aot probe.
5. Build Win32 Debug, run the core probe and the aot probe, and run pumpipx3 with
   `REPIU_TIMER_RETURN_PAD=0` to see the fatal path end in `exit(-1)` after a key.
6. Record the service in `docs/analysis/interrupts-and-port-io.md`, write the work log and commit.
