# Task 741 작업 로그 — breakpoint 지점 census와 pumpitea의 trap 부하

설계: [20260926-741](../design/20260926-741-breakpoint-site-census-and-isr-trap-load.md)
작업 지시: [20260926-741](../work-orders/20260926-741-breakpoint-site-census-and-isr-trap-load.md)

## 요약

"ISR 입력 스캔의 trap 부하"라는 전제는 틀렸습니다. Task 737 뒤로 입력 스캔은 tick당 trap 한 번이고
전체 breakpoint의 1% 남짓입니다. 새로 넣은 **breakpoint 지점 census**가 보여 준 실제 상위 지점은
Watcom C 런타임 `delay()`의 **INT 21h AH=2Ch(시각 읽기) 호출 세 곳**(전체의 61%)과 `lseek`(10%),
`push es; mov es,eax` 한 쌍(10%)이었습니다. trap의 수는 게임 자신의 대기 루프가 정하므로 줄이지
않았고, **trap 한 번의 비용**을 줄였습니다. HLE 재진입마다 같은 guest 주소를 Zydis로 세 번씩
(segment write 여부, 앞 64 명령의 span 안전, long mode 동일성) 다시 디코드하던 것을 주소별 메모로
바꾸자 breakpoint 한 번의 VEH 비용이 15.2k → 8.9k cycle(−41%), VEH의 벽시계 비중이 14.3% → 9%대가
됐습니다. DOS 서비스 안의 호출마다 `getenv`(INT 21h당 2~3회)도 정적 캐시로 바꿨습니다.

## 과정

1. **사용자 로그(36초 플레이).** 포트 I/O 40,946회, 지연 루프 batch 7,719회(tick당 1회)인데
   breakpoint 예외는 1,153,660회(초당 32,000회). 개발 머신 attract 27초도 1,035,781회. 실행 시간
   프로파일: VEH 19.5%(이후 측정에서 14.3%), breakpoint당 VEH 안 약 20,000 cycle, 그중 92.9%가
   `aot-transfer`, 그 안의 reentry `residual`이 94%.
2. **있는 계측으로는 못 갈랐습니다.** 예외 종류별 합계, 포트 주소별, cache boundary opcode별뿐.
   DOS INT trace(12초)는 INT 21h 23,587회로 breakpoint의 1/9만 설명하고(stderr 출력이 대기 루프를
   느리게 해 수치가 왜곡됨), 시간 표본 census(`REPIU_GUEST_POSITION_CENSUS`)는 Linux에서 표본을 못
   잡습니다(capture-failures 19,516/19,516).
3. **breakpoint 지점 census.** VEH 초입에서 host EIP별로 세고, cache 주소는 `FindAotGuestAddress`로
   guest 주소를 붙이며, `NoteVehExitSite`가 exit site를 누적합니다. 첫 판은 32칸 선형 표였는데
   시작 시점의 지점들이 칸을 다 채워 98%가 overflow로 갔습니다 → 512칸 open addressing으로 바꾸고
   보고 때 상위 32개를 뽑습니다.
4. **결과(attract 27초, 1,021,803회).**

   | guest 주소 | 횟수 | 정체 |
   |---|---|---|
   | `0x01101297`, `0x01101287` | 237,680 + 215,182 | Watcom `delay()` 보정: 초가 바뀔 때까지 AH=2Ch를 돌린 뒤 1초 동안 몇 번 부를 수 있는지 셈(시작 후 약 2초) |
   | `0x011012D3` | 166,376 | `delay(ms)`: 보정값 × ms 만큼 AH=2Ch를 돎(초당 6,600회) |
   | `0x01101BAE` | 102,555 | `lseek` AH=42h, `tell` (초당 3,800회) |
   | `0x010FE375`/`376` | 53,914 × 2 | memcpy helper의 `push es; mov es,eax` (segment write) |
   | `0x010F659E` | 27,594 | `jmp cs:[ebx*4+table]` — 번역되지 않은 boundary |
   | `0x01028E42` | 12,764 | ISR 입력 스캔의 batch된 `in` (tick당 1회) |
   | `0x0102AAE5`, `AB87`, `AB7D` | 6,383 × 3 | ISR의 EOI `out`, `iret` 등 |

5. **trap당 비용의 정체.** reentry 경로가 같은 주소를 매번 세 방향으로 Zydis 디코드했습니다:
   `ProbeGuestInstructionSegmentWrite`(1회), `IsImmediateHleReentrySpanSafe`(앞으로 최대 64명령),
   `CanResumeLinuxX64LegacyTarget`(`ClassifyLongModeBytes`, 재진입당 2~3회). 답은 guest 바이트와
   code segment의 default operand size, HLE boundary 목록에만 의존합니다.
6. **수정.** `ThreadContext::AotReentryMemoEntry`(1,024칸 direct-mapped, 주소·세대·첫 8바이트 지문,
   long mode 답은 code mode도 보관). 세대는 `NoteSuccessfulAotGuestWrite`마다 올라갑니다.
   `REPIU_AOT_REENTRY_MEMO=0`으로 끕니다. 첫 판은 지문이 없어 core probe `general_stack`이
   실패했습니다(probe가 같은 주소의 바이트와 code mode를 바꿔 다시 묻는데 옛 답을 돌려줌) → 지문과
   code mode를 넣어 통과.
7. **작은 것들.** `ReadLocalWallClock`의 `localtime_r`을 초당 한 번으로(스레드별 캐시, DOS 서비스
   4.0k → 3.75k cycle로 효과 미미). `REPIU_DOS_INT_TRACE`의 `getenv`를 정적 캐시로(DOS 서비스
   3.6k → 2.8k cycle).

## 검증

| 검증 | 결과 |
|---|---|
| attract 27초, 수정 전(bp2) | breakpoint 1,021,803, VEH 15.95G cycle = **15.2k/회**, VEH 비중 14.27%, reentry residual 19.7k/회 |
| 메모 뒤(bp4) | VEH 10.42G/1,219,422 = **8.5k/회**, 비중 9.32%, 메모 적중 99%(segment 1,149,989/9,672 등), reentry residual 5.0k/회 |
| getenv 캐시 뒤(bp5) | DOS 서비스 2.8k/회, VEH 8.9k/회, 비중 8.48% |
| 지문·code mode 뒤(bp6, 최종) | VEH 10.89G/1,220,692 = **8.9k/회**, 비중 9.74%, 메모 적중 99% |
| `delay()` 보정이 센 초당 호출 수 | 237,680 → 308,670~328,403 (trap이 싸진 만큼) |
| Linux x64 Release core probe | 지문 전 `general_stack` 실패 → 지문 후 `core_probe_all=true` |
| Win32 x86 Debug 빌드 + core probe | 빌드 성공, `core_probe_all=true` |
| pumpit2a 25초 | 폴트 0, 3,292 frame, 메모 적중 72~86% |
| pumpitea 플레이 60초(합성 키) | 곡 `39.AUD` 도달, MP3 multi 0·pcm-empty 0, 폴트 0. 첫 시도는 SERVICE 입력이 3회만 전달됐는데 다른 실행 직후라 창 포커스가 없던 것(단독 재실행 138회) |

fps는 바뀌지 않았습니다(2,700~3,200 frame/27초). trap은 게임의 대기 루프 안에서 일어나므로 비용을
줄인 만큼 같은 대기 안에서 더 많이 돌 뿐입니다(보정값 상승이 그 증거). 얻은 것은 CPU 점유이지
프레임이 아닙니다.

로그: `build/task741-bp2.err.log`(기준), `bp4`·`bp5`·`bp6`, `build/task741-play2.err.log`,
`build/task741-pumpit2a-reg.err.log`.

## 남은 것

* trap 한 번의 남은 8.9k cycle: prologue 1.2k, DOS 서비스 약 3k(`RecordDosSeek` 등 추적 기록),
  reentry 약 5k. 커널의 SIGTRAP 왕복은 이 수치 밖입니다.
* `jmp cs:[table]`(초당 1,100회)은 AOT가 번역하지 않는 boundary라 매번 trap입니다.
* `push es; mov es,eax`(초당 4,000회)는 selector guard가 다루지 않는 segment write입니다.
* `delay()` 보정의 2초 spin은 게임 자신의 것입니다. AH=2Ch에서 host를 재우면 없앨 수 있지만
  보정값이 달라져 `delay()`의 길이가 host 슬립 정밀도에 매이므로 하지 않았습니다.
* 시간 표본 census가 Linux에서 표본을 못 잡는 것은 고치지 않았습니다.

---

# English

# Task 741 work log — breakpoint site census and pumpitea's trap load

Design: [20260926-741](../design/20260926-741-breakpoint-site-census-and-isr-trap-load.md)
Work order: [20260926-741](../work-orders/20260926-741-breakpoint-site-census-and-isr-trap-load.md)

## Summary

The premise "the ISR input scan's trap load" was wrong: since Task 737 the input scan traps once
per tick, about 1% of all breakpoints. The new **breakpoint site census** named the real top sites:
the Watcom C runtime `delay()`'s **three INT 21h AH=2Ch (get time) calls** (61% of all breakpoints),
`lseek` (10%) and one `push es; mov es,eax` pair (10%). The number of traps is set by the game's own
wait loops, so it was left alone; **the cost of one trap** was cut. Every HLE reentry decoded the same
guest address with Zydis three ways (segment write? span safe for up to 64 instructions? identical in
long mode?); a per-address memo replaces that, and one breakpoint's VEH cost fell from 15.2k to 8.9k
cycles (−41%), the VEH's share of wall from 14.3% to about 9%. The per-call `getenv` inside the DOS
service (two or three per INT 21h) became a static cache too.

## Steps

1. **The user's log (36 s of play).** 40,946 port I/Os and 7,719 delay-loop batches (one per tick),
   yet 1,153,660 breakpoint exceptions (32,000 a second); a 27 s attract on the development machine
   1,035,781. The execution-time profile: VEH 19.5% (14.3% in later runs), about 20,000 cycles inside
   the VEH per breakpoint, 92.9% of it in `aot-transfer`, 94% of that in the reentry `residual`.
2. **The existing instruments could not separate it.** Per-kind totals, per-port-address and per
   cache-boundary-opcode only. The DOS INT trace (12 s) showed 23,587 INT 21h calls, a ninth of the
   breakpoints (its stderr writes slow the wait loop and distort the count), and the time-sampling
   census (`REPIU_GUEST_POSITION_CENSUS`) captures nothing on Linux (19,516 of 19,516 failures).
3. **Breakpoint site census.** Counted by host EIP at the VEH entry; a cache address gets its guest
   address from `FindAotGuestAddress`; `NoteVehExitSite` accumulates the exit site. The first version
   was a 32-slot list that start-up sites filled, sending 98% to overflow; it became a 512-slot
   open-addressing table with the 32 busiest reported.
4. **Result (27 s attract, 1,021,803 breakpoints).**

   | Guest address | Count | What it is |
   |---|---|---|
   | `0x01101297`, `0x01101287` | 237,680 + 215,182 | Watcom `delay()` calibration: spin AH=2Ch until the seconds change, then count calls for one second (about 2 s after start) |
   | `0x011012D3` | 166,376 | `delay(ms)`: spins AH=2Ch calibration × ms times (6,600 a second) |
   | `0x01101BAE` | 102,555 | `lseek` AH=42h, `tell` (3,800 a second) |
   | `0x010FE375`/`376` | 53,914 × 2 | a memcpy helper's `push es; mov es,eax` (segment write) |
   | `0x010F659E` | 27,594 | `jmp cs:[ebx*4+table]`, an untranslated boundary |
   | `0x01028E42` | 12,764 | the ISR input scan's batched `in` (once per tick) |
   | `0x0102AAE5`, `AB87`, `AB7D` | 6,383 × 3 | the ISR's EOI `out`, `iret` and so on |

5. **Where the per-trap cost was.** The reentry path decoded the same address three ways every time:
   `ProbeGuestInstructionSegmentWrite` (one decode), `IsImmediateHleReentrySpanSafe` (up to 64
   instructions ahead) and `CanResumeLinuxX64LegacyTarget` (`ClassifyLongModeBytes`, two or three
   times per reentry). The answers depend only on the guest bytes, the code segment's default operand
   size and the HLE boundary list.
6. **Fix.** `ThreadContext::AotReentryMemoEntry` (1,024 direct-mapped slots keyed by address with a
   generation and the first eight bytes as a fingerprint; the long-mode answer also keeps its code
   mode). The generation moves in `NoteSuccessfulAotGuestWrite`. `REPIU_AOT_REENTRY_MEMO=0` turns
   it off. The first version had no fingerprint and failed the core probe's `general_stack` (the probe
   rewrites the bytes and the code mode at one address and asks again; it got the old answer);
   the fingerprint and code mode fixed that.
7. **Small ones.** `ReadLocalWallClock` converts with `localtime_r` once per second (per-thread
   cache; DOS service 4.0k → 3.75k cycles, negligible). The `REPIU_DOS_INT_TRACE` `getenv` calls
   became static caches (DOS service 3.6k → 2.8k cycles).

## Verification

| Check | Result |
|---|---|
| 27 s attract, before (bp2) | 1,021,803 breakpoints, VEH 15.95G cycles = **15.2k each**, VEH share 14.27%, reentry residual 19.7k each |
| After the memo (bp4) | VEH 10.42G / 1,219,422 = **8.5k each**, share 9.32%, memo hits 99% (segment 1,149,989 / 9,672 misses), reentry residual 5.0k |
| After the getenv cache (bp5) | DOS service 2.8k each, VEH 8.9k each, share 8.48% |
| After the fingerprint and code mode (bp6, final) | VEH 10.89G / 1,220,692 = **8.9k each**, share 9.74%, memo hits 99% |
| Calls per second counted by `delay()`'s calibration | 237,680 → 308,670–328,403 (as much as the traps got cheaper) |
| Linux x64 Release core probe | `general_stack` failed before the fingerprint → `core_probe_all=true` after |
| Win32 x86 Debug build + core probe | build succeeded, `core_probe_all=true` |
| pumpit2a 25 s | no faults, 3,292 frames, memo hits 72–86% |
| pumpitea 60 s play (synthetic keys) | reached song `39.AUD`, MP3 multi 0 and pcm-empty 0, no faults. One attempt delivered only 3 SERVICE presses because the window had no focus right after another run (138 on a standalone rerun) |

Frame rate did not change (2,700–3,200 frames per 27 s): the traps happen inside the game's wait
loops, so a cheaper trap only spins more inside the same wait, which the rising calibration count
shows. The gain is CPU occupancy, not frames.

Logs: `build/task741-bp2.err.log` (baseline), `bp4`, `bp5`, `bp6`, `build/task741-play2.err.log`,
`build/task741-pumpit2a-reg.err.log`.

## What remains

* The 8.9k cycles left per trap: prologue 1.2k, DOS service about 3k (`RecordDosSeek` and other
  trace records), reentry about 5k. The kernel's SIGTRAP round trip is outside these numbers.
* `jmp cs:[table]` (1,100 a second) is a boundary the AOT does not translate, so it traps every time.
* `push es; mov es,eax` (4,000 a second) is a segment write the selector guard does not cover.
* `delay()`'s two-second calibration spin is the game's own. Sleeping the host inside AH=2Ch would
  remove it, but the calibration would then tie `delay()`'s length to the host's sleep granularity,
  so it was not done.
* The time-sampling census still captures nothing on Linux.
