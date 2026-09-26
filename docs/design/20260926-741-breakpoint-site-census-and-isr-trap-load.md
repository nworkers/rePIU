# Task 741: pumpitea의 trap 부하 — breakpoint 지점 census와 ISR 입력 스캔

## 한국어

### 배경

Task 735가 pumpitea의 입력 스캔(`0x01028E30`, 타이머 ISR에서 tick마다 호출)이 200회 `in ax,dx`
지연 루프를 trap으로 실행해 88초에 포트 읽기 405만 회, breakpoint 예외 505만 회를 낸다고 기록했다.
Task 737이 그 루프의 `jge exit; jmp back` 형식을 지연 루프 batcher가 받아들이게 해 포트 읽기는
tick당 한 번으로 줄었다(사용자 로그 36초: 포트 I/O 40,946회, batch 7,719회). 그러나 같은 로그의
breakpoint 예외는 1,153,660회(초당 32,000회)로 남아 있고, 개발 머신의 attract 27초에서도
1,035,781회(초당 38,000회)다. 실행 시간 프로파일은 VEH가 벽시계의 19.5%, breakpoint 하나의 VEH
안 비용이 약 20,000 cycle(그중 92.9%가 `aot-transfer`)이라고 말한다.

무엇이 trap을 내는지는 아직 모른다. 있는 census는 예외 종류별 합계(단일 스텝/breakpoint/AV),
포트 I/O 주소별, AOT cache 안의 boundary opcode별뿐이고, **breakpoint를 낸 주소별 census는 없다.**
DOS INT trace(12초)는 INT 21h 23,587회(`lseek` AH=42 13,725회, 시각 AH=2C 8,468회)와 segment
load/store 24,208회를 보여 주지만 합쳐도 초당 4,000회로 breakpoint의 1/9이다. 시간 표본 census
(`REPIU_GUEST_POSITION_CENSUS`)는 Linux에서 표본을 못 잡는다(capture-failures 19,516/19,516).

### 설계 — 계측 먼저

1. **breakpoint 지점 census.** VEH 초입의 `RecordVehExceptionCensus`에서 breakpoint 예외마다
   host EIP를 32칸 표에 세고, 넘치면 overflow를 센다. 첫 관측 때 cache 주소면
   `FindAotGuestAddress`로 guest 주소를, arena 주소면 그 주소 자체를 guest 주소로 적는다.
   `NoteVehExitSite`가 그 예외를 끝낸 exit site를 같은 칸에 4종까지 누적한다. 최종 보고가
   상위 16개를 `host/guest/cache/count/exits`로 찍는다. 항상 켜져 있고 비용은 32칸 선형 탐색이다
   (포트 I/O 주소 census와 같은 규모).
2. 그 표로 pumpitea attract 30초를 읽고, 상위 지점의 guest 코드를 디스어셈블해 무엇인지 확정한다.

### 설계 — 수정 (측정 결과에 따라)

상위 지점이 무엇이냐에 따라 갈린다. 후보와 대응은 다음과 같다.

| 상위 지점이 | 대응 |
|---|---|
| ISR의 `sti`/`iret`/`pop ds..gs`/EOI `out` | ISR 전체를 하나의 HLE로 묶을 수 없으므로 개별 명령의 비용(`aot-transfer` 19k cycle)을 줄이는 쪽 |
| DOS `lseek`/시각 호출 | 호출 지점 확인 후 게임 로직 변경 없이 HLE 경로 단축 |
| segment load/store | Task 4xx의 selector guard 확장 |
| 다른 것 | 그때 설계 |

### 검증 전략

수정 전후로 같은 30초 attract에서 breakpoint 예외 수와 census 상위 지점, fps를 비교한다.

## English

### Background

Task 735 recorded that pumpitea's input scan (`0x01028E30`, called from the timer ISR every tick)
ran its 200-iteration `in ax,dx` delay loop through traps: 4.05 million port reads and 5.05 million
breakpoint exceptions in 88 s. Task 737 made the delay-loop batcher accept its `jge exit; jmp back`
form, and port reads fell to one per tick (the user's 36 s log: 40,946 port I/Os, 7,719 batches).
But the same log still shows 1,153,660 breakpoint exceptions (32,000 a second), and a 27 s attract
run on the development machine 1,035,781 (38,000 a second). The execution-time profile puts the VEH
at 19.5% of wall and one breakpoint at about 20,000 cycles inside the VEH, 92.9% of it in
`aot-transfer`.

What raises them is unknown. The existing censuses are per exception kind, per port I/O address and
per boundary opcode inside the AOT cache; **there is no census by breakpoint address.** The DOS INT
trace (12 s) shows 23,587 INT 21h calls (`lseek` AH=42 13,725, time AH=2C 8,468) and 24,208 segment
loads/stores, together 4,000 a second, a ninth of the breakpoints. The time-sampling census
(`REPIU_GUEST_POSITION_CENSUS`) captures nothing on Linux (19,516 of 19,516 failures).

### Design — instrumentation first

1. **Breakpoint site census.** `RecordVehExceptionCensus` at the VEH entry counts each breakpoint's
   host EIP in a 32-slot table, with an overflow count. On first sight a cache address is mapped to
   its guest address with `FindAotGuestAddress`; an arena address is its own guest address.
   `NoteVehExitSite` accumulates, per slot, up to four exit sites that ended the exception. The
   final report prints the top 16 as `host/guest/cache/count/exits`. Always on; the cost is a
   32-slot linear scan, the same as the port I/O address census.
2. Read pumpitea's 30 s attract through that table and disassemble the top sites.

### Design — the fix, decided by the measurement

| If the top sites are | Then |
|---|---|
| the ISR's `sti`/`iret`/`pop ds..gs`/EOI `out` | the ISR cannot be one HLE, so cut the per-instruction cost (`aot-transfer`, 19k cycles) |
| DOS `lseek`/time calls | find the callers and shorten the HLE path without touching game logic |
| segment loads/stores | extend the selector guard |
| something else | design then |

### Verification strategy

The same 30 s attract before and after: breakpoint count, the census's top sites, and fps.

---

## 결과 (구현 후) / Result (after implementation)

### 한국어

census의 상위 지점은 표의 어느 행도 아닌 "다른 것"이었습니다. Watcom `delay()`의 INT 21h AH=2Ch
호출(보정 spin 2초 + `delay()` 자체, 전체의 61%), `lseek`(10%), memcpy helper의 segment write
쌍(10%). ISR은 1%대입니다. 그래서 trap의 수가 아니라 **trap 한 번의 비용**을 줄였습니다. HLE
재진입이 같은 주소를 세 방향으로 다시 Zydis 디코드하던 것을 주소별 메모(1,024칸, 세대·8바이트
지문·code mode)로 바꾸자 breakpoint당 VEH 비용 15.2k → 8.9k cycle, VEH 벽시계 비중 14.3% → 9%대.
fps는 그대로입니다(trap이 게임의 대기 루프 안에 있으므로). 상세는 작업 로그에 있습니다.

### English

The census's top sites were the table's "something else": Watcom `delay()`'s INT 21h AH=2Ch calls
(a two-second calibration spin plus `delay()` itself, 61% of all), `lseek` (10%) and a memcpy
helper's segment-write pair (10%); the ISR is about 1%. So the cost of one trap was cut rather than
the count: the HLE reentry's three Zydis decodes of the same address became a per-address memo
(1,024 slots with generation, eight-byte fingerprint and code mode), taking one breakpoint's VEH cost
from 15.2k to 8.9k cycles and the VEH's share of wall from 14.3% to about 9%. Frame rate is
unchanged, since the traps sit inside the game's wait loops. Details in the work log.
