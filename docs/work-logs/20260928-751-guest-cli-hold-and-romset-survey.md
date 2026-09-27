# Task 751 작업 로그 — 롬셋 전수 조사와 게스트 `cli` 보존

설계: [20260928-751](../design/20260928-751-guest-cli-hold-and-romset-survey.md)
작업 지시: [20260928-751](../work-orders/20260928-751-guest-cli-hold-and-romset-survey.md)

## 요약

roms의 22개 프로필을 Linux x64와 Win32에서 60초씩 돌렸습니다. 6개(pumpit2, pumpit3, pumpite, pumpitp3a,
pumpipx3a, pumpipx3b)는 CHD 폴더가 없어 실행할 수 없고(자산 부재), 나머지 16개 가운데 **Linux x64에서는
pumpitpc**가, **Win32에서는 pumpitp3·pumpipx3**가 실행 중 죽었습니다. 수정 뒤 Linux x64는 16개 모두 60초를
완주합니다. Win32의 두 개는 원인이 다르고(게임의 "unable to find entry point in DLL") 고치지 못했습니다.

Linux x64 pumpitpc의 원인은 둘이었습니다. (1) 게임이 `cli`…`sti`로 감싼 보안 칩(CAT702) 통신 한가운데
타이머 tick이 주입돼, 핸들러가 보드의 destination 레지스터를 바꿔 놓고 검사가 실패했습니다. 엔진은 게스트의
`cli`를 보존하지 못하고 있었습니다(user mode에서는 IF를 내릴 수 없음). (2) 그 검사를 넘기자 첫 점수 표시의
`aam 0xa`에서 죽었습니다(long mode에 없는 명령, 에뮬레이션 없음). cli hold를 넣는 과정에서 Task 748이 본
"tick 폭주"(시작 직후 초당 18,000 tick, 프레임 0장)의 기전도 드러나 함께 막았습니다.

## 과정

1. **조사 도구**. `scripts/survey_romsets.sh`가 프로필마다 종료 코드·프레임·fps·5초 간격 화면 샘플·폴트·자산
   열기를 `summary.txt`로 모읍니다. 입력은 `survey_generic.txt`(SERVICE 5회, 양쪽 center, 방향 패드).
2. **죽는 자리**. 세 롬셋 모두 `mov ah,8; int 21h`(Watcom `getch`). pumpitpc의 호출 경로는 시작 시 검사
   루프(`0x01010B84`): 검사 함수(`0x01019818`)를 500번 부르고 한 번이라도 0이면 오류 화면 → 3초 → `getch`.
3. **키는 맞다**. 검사는 100개 챌린지 표(`0xB542`, 17바이트 칸)와 기대 응답 표(`0xBBE6`)를 씁니다. 통신
   루틴(`0x010195BE`)을 그대로 옮긴 `scripts/cat702_table_check.py`에서 pumpitpc 100/100, 다른 CAT702 롬셋도
   모두 자기 키로 100/100. 통신 루틴은 `cli`(`0x010EC758`)로 시작해 `sti`(`0x010EC77F`)로 끝납니다.
4. **cli hold**. `cli` HLE가 세우고 `sti` HLE나 주입 프레임의 `iret`가 내리며, 서 있는 동안 주입을 미룹니다.
   100 ms 밸브. 첫 실행에서 새 죽음: 미뤄진 tick이 닫는 `sti`에 몰리자, dispatcher의 privileged 체인이 주입
   뒤 ISR의 게스트 주소(`pusha`로 시작)로 그대로 재개해 SIGILL. `CanEnterTimerInterruptHandler` 확인과 cache
   진입을 넣었습니다.
5. **`aam`**. pumpitpc가 55초까지 가서 `0x010F1F98 aam 0xa`에서 SIGTRAP. `HandleAsciiAdjustInstruction` 추가.
6. **tick 폐기와 폭주**. hold 아래 폐기가 늘어(`sti` 연쇄 규칙이 반환 뒤의 `sti`까지 막음) 규칙을 핸들러가
   돌아오기 전으로 좁히고 `iret` 뒤 주입을 넣자, 51.9 kHz 단계에서 핸들러가 끝없이 이어져 게임이 PIT를
   240 Hz로 되돌리지 못했습니다(초당 18,000 tick). 차례로: 반환 직후 연쇄를 3개·주기 1 ms 이상으로 제한 →
   ISR이 체인하는 옛 INT 8 핸들러의 `iret`를 ISR의 반환으로 읽던 것을 주입 프레임 일치로 교정 → 주입 프레임
   스택으로 중첩을 한 단계로 제한 → 직전 핸들러가 쓴 시간만큼 인터럽트된 코드에 차례를 줌. 마지막 규칙 뒤
   32회 시작에서 폭주 0회.
7. **Win32**. 재조사에서 pumpitp3·pumpipx3는 그대로 죽었습니다. CAT702 트레이스를 표와 대조하니 통신 600개
   모두 정상이고, 게임이 남긴 콘솔 메시지는 `Fatal error: unable to find entry point in DLL.`, 호출 경로는
   게임의 fatal 처리기(`0x0102D840`)였습니다. 조회 실패 이름을 찍는 진단(`[repiu-linexe] get-proc miss`)을
   넣었습니다.

## 검증

| 검증 | 결과 |
|---|---|
| Linux x64 전수 조사, 수정 전(v0.0.193) | 16개 중 15개 완주, pumpitpc 5초에 SIGTRAP(`getch`) |
| Linux x64 전수 조사, 수정 후 | 16개 모두 60초 완주, 폴트 0, hold 만료 0, 최대 hold 4~14 ms |
| Linux x64 네 롬셋 8회씩 시작 | tick 폭주 0/32 (PIT 240 Hz 도달 32/32); 3회는 예산 만료 뒤 teardown segfault(기존 결함, Task 730) |
| pumpitpc 부팅 검사 | CAT702 통신 501회, hold `cli/sti` 13,953/13,954, blocked 10,395 |
| tick 폐기(Linux x64, 60초) | 대부분 33~114(이전 40~118); pumpitpc 462·pumpitpr 409는 부팅 검사 1~2초 구간에 몰림, 이후 초당 243 |
| Win32 전수 조사, 수정 전 → 후 | 14개 완주 그대로; pumpitp3·pumpipx3는 전후 모두 죽음(DLL entry point) |
| Linux x64 Release core probe | `core_probe_all=true`, `pic_timer_in_service` 19개 항목 true |
| Win32 x86 Debug 빌드 + core probe | 아래 English 절과 같음 |

로그: `build/survey/linux-x64/`, `build/survey/linux-x64-751c/`, `build/survey/win32/`,
`build/survey/win32-751/`, `build/task751-*.err.log`.

## 남은 것

* **Win32의 pumpitp3·pumpipx3**: 게임이 `Fatal error: unable to find entry point in DLL.`을 찍고 fatal 처리기에서 `getch`로 갑니다. 간헐적입니다:
  pumpitp3는 조사 환경(화면 샘플·자산 트레이스·입력 스크립트)에서 2회 모두 죽고 일반 실행에서는 4회 모두
  통과했으며, pumpipx3는 3회 모두 죽었습니다. Glide 이름 조회 실패(`get-proc miss`)는 찍히지 않았고 마지막
  성공 조회는 36~38번째입니다(Linux는 40번째까지). CAT702 통신은 정상입니다. 원인 미확정.
* CHD가 없는 6개 프로필은 롬 폴더에 디스크 이미지를 넣어야 조사할 수 있습니다.
* INT 21h AH=08h(`getch`)는 여전히 없습니다. 검사가 통과하면 닿지 않지만, 닿으면 이름 없는 폴트로 죽습니다.
* pumpitpc·pumpitpr의 부팅 검사 구간 tick 폐기(약 450): 통신 하나가 엔진에서 약 10 ms라 그 사이 tick
  2~3개 가운데 하나만 닫는 `sti`에 들어갑니다. 게임 진행 중에는 영향이 없습니다.
* Win32는 `iret`가 native라 핸들러 반환을 보지 못합니다. 중첩·차례 규칙은 꺼져 있고 hold는 `sti`나 100 ms
  밸브로만 끝납니다(최대 hold 37~137 ms 관측). Win32의 tick 폐기는 전후 모두 많습니다(Debug 빌드).
* 예산 만료 뒤 teardown segfault(rip 0x2002xx)는 그대로입니다.

---

# English

# Task 751 work log — the ROM set survey and keeping the guest's `cli`

Design: [20260928-751](../design/20260928-751-guest-cli-hold-and-romset-survey.md)
Work order: [20260928-751](../work-orders/20260928-751-guest-cli-hold-and-romset-survey.md)

## Summary

The 22 profiles in roms ran for 60 s each on Linux x64 and Win32. Six (pumpit2, pumpit3, pumpite,
pumpitp3a, pumpipx3a, pumpipx3b) have no CHD directory and cannot run (assets absent); of the other 16,
**pumpitpc died on Linux x64** and **pumpitp3 and pumpipx3 died on Win32**. After the fixes all 16 run
their 60 s on Linux x64. The two on Win32 have a different cause (the game's "unable to find entry point
in DLL") and are not fixed.

pumpitpc on Linux x64 had two causes. (1) A timer tick was injected in the middle of the security chip
(CAT702) transaction the game wraps in `cli`…`sti`; the handler moved the board's destination register
and the check failed. The engine was not keeping the guest's `cli` (IF cannot be cleared in user mode).
(2) Past that check it died at the first score's `aam 0xa` (no such instruction in long mode, and no
emulation). Adding the cli hold also exposed the mechanism of the "tick storm" Task 748 saw (18,000 ticks
a second right after the start and no frame), which is closed with it.

## Steps

1. **The survey tool**. `scripts/survey_romsets.sh` collects per profile the exit code, frames, fps, a
   screen sample every 5 s, faults and asset opens into `summary.txt`, driven by `survey_generic.txt`
   (five SERVICE credits, both centers, direction pads).
2. **Where they die**. All three at `mov ah,8; int 21h` (Watcom's `getch`). pumpitpc reaches it from the
   start-up check loop (`0x01010B84`): the check (`0x01019818`) is called 500 times and one zero draws the
   error screen, waits three seconds and calls `getch`.
3. **The key is right**. The check uses a table of 100 challenges (`0xB542`, 17-byte slots) and one of
   expected responses (`0xBBE6`). `scripts/cat702_table_check.py`, which reproduces the transaction routine
   (`0x010195BE`), matches pumpitpc 100/100, and every other CAT702 ROM set 100/100 with its own key. The
   routine starts with `cli` (`0x010EC758`) and ends with `sti` (`0x010EC77F`).
4. **The cli hold**. Set by the `cli` HLE, ended by the `sti` HLE or the injected frame's `iret`, deferring
   injection in between, with the 100 ms valve. The first run died a new way: with the held ticks arriving
   at the closing `sti`, the dispatcher's privileged chain resumed at the ISR's guest address (it begins
   with `pusha`) and raised SIGILL. `CanEnterTimerInterruptHandler` and an entry through the cache fixed it.
5. **`aam`**. pumpitpc ran to 55 s and died with SIGTRAP at `0x010F1F98 aam 0xa`.
   `HandleAsciiAdjustInstruction` was added.
6. **Dropped ticks, and the storm**. Drops rose under the hold (the `sti` chain rule also held back a
   `sti` after the handler's return), so the rule was narrowed to before the return and an injection after
   `iret` was added; at the 51.9 kHz stage handlers then followed each other without end and the game
   never set the PIT back to 240 Hz (18,000 ticks a second). In order: the chain after a return bounded to
   three and to periods of 1 ms or more; the `iret` of the previous INT 8 handler the ISR chains to, which
   had been read as the ISR's return, told apart by the injected frame; nesting bounded to one level by
   the stack of injected frames; the interrupted code given a turn as long as the last handler took. After
   the last rule, no storm in 32 starts.
7. **Win32**. pumpitp3 and pumpipx3 still died in the second survey. Their CAT702 trace matches the
   tables in all 600 transactions; the message the game left on its console is `Fatal error: unable to
   find entry point in DLL.` and the path is the game's fatal handler (`0x0102D840`). A diagnostic naming a
   failed lookup (`[repiu-linexe] get-proc miss`) was added.

## Verification

| Check | Result |
|---|---|
| Linux x64 survey before (v0.0.193) | 15 of 16 ran; pumpitpc SIGTRAP at 5 s (`getch`) |
| Linux x64 survey after | all 16 ran 60 s, no faults, no expired hold, longest hold 4–14 ms |
| Four ROM sets started eight times each on Linux x64 | no tick storm in 32 (the PIT reached 240 Hz in 32); three teardown segfaults after the budget (the existing defect, Task 730) |
| pumpitpc's start-up check | 501 CAT702 transactions, hold `cli/sti` 13,953/13,954, blocked 10,395 |
| Dropped ticks (Linux x64, 60 s) | mostly 33–114 (40–118 before); pumpitpc 462 and pumpitpr 409, inside the 1–2 s of the start-up check, 243 a second afterwards |
| Win32 survey before → after | the same 14 ran; pumpitp3 and pumpipx3 died both times (DLL entry point) |
| Linux x64 Release core probe | `core_probe_all=true`, the 19 `pic_timer_in_service` items true |
| Win32 x86 Debug build + core probe | build succeeded (0 errors), `core_probe_all=true` |

Logs: `build/survey/linux-x64/`, `build/survey/linux-x64-751c/`, `build/survey/win32/`,
`build/survey/win32-751/`, `build/task751-*.err.log`.

## What remains

* **pumpitp3 and pumpipx3 on Win32**: the game prints `Fatal error: unable to find entry point in DLL.` and goes
  through its fatal handler to `getch`. It is intermittent: pumpitp3 died both times under the survey's
  environment (screen samples, asset trace, input script) and ran all four times without it; pumpipx3
  died three times out of three. No Glide name lookup failed (`get-proc miss` never printed) and the last
  successful lookup is the 36th to 38th (Linux reaches the 40th). The CAT702 transactions are correct.
  Cause not established.
* The six profiles without a CHD need their disc images in the ROM folder before they can be surveyed.
* INT 21h AH=08h (`getch`) is still absent. A passing check never reaches it; reaching it is an unnamed
  fault.
* Ticks dropped during pumpitpc's and pumpitpr's start-up check (about 450): one transaction takes about
  10 ms in the engine, and of the two or three ticks owed in it one goes in at the closing `sti`. Play is
  not affected.
* Win32's `iret` is native, so a handler's return is not seen there: the nesting and turn rules are off
  and a hold ends only at `sti` or the 100 ms valve (holds of 37–137 ms observed). Win32 drops many ticks
  before and after (a Debug build).
* The teardown segfault after the budget (rip 0x2002xx) is unchanged.
