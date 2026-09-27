# Task 751: 롬셋 전수 조사와 게스트 `cli` 보존

## 한국어

### 배경

사용자 요청: "현재 roms의 버전 중 실행이나 진행이 불가능한 버전이 있는지 전수 조사해. 각 1분씩 테스트해
보면 될 것 같다." 22개 프로필을 Linux x64와 Win32에서 60초씩 돌린 결과(공통 입력 스크립트, 5초 간격 화면
샘플, v0.0.193):

| | Linux x64 | Win32 |
|---|---|---|
| 실행·진행 | 15 | 14 |
| 실행 중 죽음 | pumpitpc | pumpitp3, pumpipx3 |
| CHD 폴더 없음(자산 부재) | pumpit2, pumpit3, pumpite, pumpitp3a, pumpipx3a, pumpipx3b | 같음 |

죽는 셋은 같은 자리에서 죽는다: `mov ah,8; int 21h`(Watcom `getch`). 게임이 시작할 때 보안 칩(CAT702)에
챌린지 100개 중 하나를 보내고 응답을 실행 파일 안의 기대표와 비교하는 검사를 500번 돌리는데, 하나라도
틀리면 오류 화면을 그리고 3초 뒤 키 입력을 기다린다. 엔진에는 INT 21h AH=08h가 없어 거기서 죽는다.

키와 알고리즘은 맞다. 게임의 통신 절차(select low, 비트마다 clock low → data = NOT bit → clock high →
샘플, 두 비트 지연, LSB first)를 그대로 옮긴 오프라인 검사(`scripts/cat702_table_check.py`)에서 pumpitpc의
표 100개가 자기 키(`f01cfe03814038f8`)로 100/100 일치했다. 호스트마다 다른 롬셋이 실패하고 같은 롬셋도
실행마다 달라지므로(pumpitpc는 hold 없이도 한 번은 통과) 번역이 아니라 **경합**이다.

게임은 통신을 `cli` … `sti`로 감싼다(`0x010EC758` = `cli; ret`, `0x010EC77F` = `sti; ret`). 통신은 보드의
destination 레지스터(0x2D4/0x2D6)를 0x010(쓰기)과 0x008(읽기)로 번갈아 바꾸는데, 타이머 ISR도 MP3 상태를
읽으려고 같은 레지스터를 바꾼다. 실제 기계에서는 `cli`가 ISR을 막는다. 엔진에서는 게스트가 user mode라
`cli`를 저장된 context의 IF에 반영해도 커널이 되돌려 놓으므로(Task 735의 주석이 이미 적은 사실) 주입이
IF를 보고 판단할 수 없었고, tick이 통신 한가운데 들어가 destination을 바꿔 놓았다.

### 설계

1. **게스트 `cli` 보존(cli hold)**. `PicTimerInService`에 `cli_hold`를 둔다. 에뮬레이트된 `cli`가 세우고,
   `sti` 또는 주입한 프레임의 `iret`가 내린다. 서 있는 동안 `InjectPendingInterrupts`는 주입하지 않고
   미룬다. `sti`는 기존대로 다음 주입 시도를 요청하므로 밀린 tick은 닫는 `sti`에서 온다.
2. **안전 밸브**. `popfd`와, i386의 native `iret`는 엔진이 보지 못한 채 IF를 복원한다. hold가 100 ms를
   넘으면 끝난 것으로 보고 풀며 횟수를 센다. 시계가 멈추는 것보다 한 번 늦는 편이 낫다.
3. **주입 뒤 진입 가능 여부(Linux x64)**. hold로 tick이 닫는 `sti`에 몰리자 잠복 결함이 드러났다: dispatcher의
   privileged HLE 체인은 주입 뒤 ISR의 게스트 주소로 그대로 재개했고, `pusha`로 시작하는 ISR은 long mode에서
   illegal instruction으로 죽었다. 주입 전에 `CanEnterTimerInterruptHandler`(cache에 번역이 있거나 long mode
   동일)로 확인하고, 주입 뒤에는 `iret` 분기와 같은 방식으로 cache에 진입한다. Task 747의 planner-HLE 주입도
   같은 확인을 거친다.
4. **핸들러 반환과 중첩**. 주입한 프레임의 ESP를 `handler_frames`에 쌓고, 그 프레임을 pop하는 `iret`만
   반환으로 센다. ISR이 예전 INT 8 핸들러로 체인하면 그 핸들러의 `iret`가 ISR 몸통 안에서 실행되는데, 이를
   ISR의 반환으로 읽으면 닫는 `sti`가 핸들러를 끝없이 연쇄시킨다. 반환이 보이는 호스트(`iret`가
   에뮬레이트되는 Linux x64)에서는 tick이 **인터럽트된 코드**(선 프레임 없음)나 **한 단계 깊이의 닫는
   `sti`**에만 들어간다. `iret`가 native로 도는 호스트에서는 규칙을 켜지 않는다(스택 비교만으로는 인터럽트된
   코드가 프레임보다 깊이 들어간 경우를 가릴 수 없어 시계가 멈출 수 있다).
5. **`sti` 연쇄 규칙은 핸들러가 돌아오면 끝난다**. 기존 규칙(두 `sti` 전달이 연달아 올 수 없음)은 핸들러 자신의
   닫는 `sti`를 위한 것이다. 핸들러가 반환한 뒤의 `sti`는 인터럽트된 코드의 것이므로 막지 않는다. hold 아래
   `cli`…`sti` 구간이 연달아 있으면 다른 전달 시점이 없어, 이전 규칙은 tick 하나 뒤로 backlog가 넘칠 때까지
   아무것도 넣지 않았다.
6. **반환 직후 연쇄와 메인 코드의 차례**. 핸들러가 돌아온 직후 밀린 tick을 바로 넣되 3개까지만, 그리고
   tick 주기가 1 ms 이상일 때만 넣는다(`TimerInjectionSite::kAfterHandlerReturn`). 그 밖의 주입은 직전
   핸들러가 쓴 시간만큼 인터럽트된 코드가 돈 뒤에야 들어간다. 에뮬레이트된 핸들러(약 55 µs, 최대 수 ms)는
   pumpitea 계열이 잠깐 거치는 51.9 kHz 단계의 주기(19 µs)보다 느려, 제한이 없으면 핸들러만 돌고 게임은 PIT를
   240 Hz로 되돌리는 명령에 도달하지 못한다(초당 18,000 tick, 프레임 0장: Task 748이 본 "tick 폭주").
7. **`AAM`/`AAD`**. long mode에 없는 명령이라 cache가 경계로 남기는데 아무도 처리하지 않았다. Watcom의 숫자
   포맷이 `aam 0xa`로 10으로 나누고, pumpitpc는 첫 점수 표시에서 죽었다. HLE가 AL/AH와 SF·ZF·PF를 계산한다.
8. **계측과 도구**. 최종 보고 `guest cli hold …`, `timer IRQ0 after-return …`, `timer IRQ0 nesting …`,
   `timer IRQ0 turn …`. 스위치 `REPIU_GUEST_CLI_HOLD=0`. `REPIU_PIU10_CAT702_TRACE=1`은 통신 하나의
   입력·출력 비트를 select가 오를 때 찍는다(처음 600개). `scripts/survey_romsets.sh <binary> <tag> [초]
   [프로필…]`과 `scripts/input_scripts/survey_generic.txt`는 프로필마다 종료 코드, 프레임 수, fps, 화면 샘플의
   변화, 폴트, 자산 열기를 `build/survey/<tag>/summary.txt`에 모은다.

### 검증 전략

pumpitpc 반복 실행(이전: 5초에 죽음), 네 롬셋 8회씩 시작(tick 폭주 여부), 16개 실행 가능 롬셋 재조사(Linux
x64, Win32), probe(`pic_timer_in_service`)에 hold·반환·중첩·차례 케이스 추가, core probe(Linux·Win32).

## English

### Background

The user's request: "Survey every ROM version in roms for ones that cannot run or progress; a minute
each should do." The 22 profiles, 60 s each on Linux x64 and Win32 (one generic input script, a screen
sample every 5 s, v0.0.193):

| | Linux x64 | Win32 |
|---|---|---|
| Runs and progresses | 15 | 14 |
| Dies while running | pumpitpc | pumpitp3, pumpipx3 |
| No CHD directory (asset absent) | pumpit2, pumpit3, pumpite, pumpitp3a, pumpipx3a, pumpipx3b | the same |

The three die at the same place: `mov ah,8; int 21h` (Watcom's `getch`). At start-up the game sends its
security chip (CAT702) one of 100 challenges and compares the response with a table in the executable,
500 times over; one mismatch draws an error screen and, three seconds later, waits for a key. The engine
has no INT 21h AH=08h and dies there.

The key and the algorithm are right. An offline check that reproduces the game's own transaction (select
low; per bit clock low, data = NOT bit, clock high, sample; a two-bit lag; LSB first),
`scripts/cat702_table_check.py`, matches pumpitpc's 100 entries 100/100 with its own key
(`f01cfe03814038f8`). Different ROM sets fail on different hosts and the same one varies from run to run
(pumpitpc passed once without the hold), so this is a **race**, not a translation.

The game wraps the transaction in `cli` … `sti` (`0x010EC758` = `cli; ret`, `0x010EC77F` = `sti; ret`).
The transaction flips the board's destination register (0x2D4/0x2D6) between 0x010 (write) and 0x008
(read), and the timer ISR changes the same register to read the MP3 status. On the real machine `cli`
keeps the ISR out. In the engine the guest is in user mode, where a `cli` written into the saved
context's IF is undone by the kernel (Task 735's comment already says so), so injection could not judge
by IF, and a tick landed in the middle of the transaction and moved the destination.

### Design

1. **Keep the guest's `cli` (the cli hold)**. `PicTimerInService` gains `cli_hold`, set by an emulated
   `cli` and ended by `sti` or by the `iret` of an injected frame. While it holds,
   `InjectPendingInterrupts` defers. `sti` still requests the next injection attempt, so the owed ticks
   arrive at the closing `sti`.
2. **A safety valve**. `popfd`, and on i386 a native `iret`, restore IF unseen by the engine. A hold older
   than 100 ms is taken to have ended and is released, counted. One late tick is better than a stopped
   clock.
3. **Whether the handler can be entered (Linux x64)**. With the hold sending ticks to the closing `sti`, a
   latent defect appeared: the dispatcher's privileged HLE chain resumed at the ISR's guest address as it
   was, and an ISR beginning with `pusha` died as an illegal instruction in long mode. The injection is now
   preceded by `CanEnterTimerInterruptHandler` (a translation in the cache, or bytes identical in long
   mode) and followed by an entry through the cache, as the `iret` branch does. Task 747's planner-HLE
   injection takes the same check.
4. **A handler's return, and nesting**. The ESP of every injected frame is kept in `handler_frames`, and
   only the `iret` that pops that frame counts as its return. An ISR that chains to the previous INT 8
   handler sees that handler's `iret` inside its own body; taking it for the ISR's own let the closing
   `sti` chain handlers without end. On a host that sees the return (Linux x64, where `iret` is emulated) a
   tick goes only into **interrupted code** (no frame standing) or, **one level deep, at a handler's
   closing `sti`**. Where `iret` runs natively the rule stays off: the stack test alone cannot tell
   interrupted code that ran deeper than the frame, and the clock would stop.
5. **The `sti` chain rule ends with the handler**. The existing rule (two `sti` deliveries may not follow
   each other) is about a handler's own closing `sti`. A `sti` after the handler has returned belongs to
   the interrupted code and is not held back. Under the hold, back-to-back `cli`…`sti` sections leave no
   other moment to deliver, and the old rule put in one tick and then none until the backlog overflowed.
6. **The chain after a return, and the interrupted code's turn**. A tick owed when a handler returns goes
   in at once, but at most three in a row and only at a tick period of 1 ms or more
   (`TimerInjectionSite::kAfterHandlerReturn`). Any other injection waits until the interrupted code has
   run for as long as the last handler took. An emulated handler (about 55 µs, up to milliseconds) is
   slower than the period (19 µs) of the 51.9 kHz stage the pumpitea family passes through, and without
   the limit only handlers ran and the game never reached the instruction that sets the PIT back to 240 Hz
   (18,000 ticks a second and no frame: the "tick storm" Task 748 saw).
7. **`AAM`/`AAD`**. Long mode has neither, the cache leaves them as a boundary, and nothing answered it.
   Watcom's number formatting divides by ten with `aam 0xa`, and pumpitpc died at its first score. The HLE
   computes AL/AH and SF, ZF, PF.
8. **Instruments and tools**. The final report's `guest cli hold …`, `timer IRQ0 after-return …`,
   `timer IRQ0 nesting …` and `timer IRQ0 turn …`; the switch `REPIU_GUEST_CLI_HOLD=0`;
   `REPIU_PIU10_CAT702_TRACE=1` prints one transaction's input and output bits when select rises (the
   first 600). `scripts/survey_romsets.sh <binary> <tag> [seconds] [profile…]` with
   `scripts/input_scripts/survey_generic.txt` collects, per profile, the exit code, frames, fps, the change
   across screen samples, faults and asset opens into `build/survey/<tag>/summary.txt`.

### Verification strategy

pumpitpc repeated (it died at 5 s before), four ROM sets started eight times each (the tick storm), the 16
runnable ROM sets surveyed again (Linux x64, Win32), hold, return, nesting and turn cases in the
`pic_timer_in_service` probe, the core probes (Linux, Win32).
