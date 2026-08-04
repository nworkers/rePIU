# Task 414 작업 로그 — 포트 I/O 지연 루프 batching (**멈춤 해소**)

설계: [20260804-414](../design/20260804-414-port-io-delay-loop-batching.md) ·
작업 지시: [20260804-414](../work-orders/20260804-414-port-io-delay-loop-batching.md)

## 1. 한 줄 결과

**pumpit3가 다시 그립니다.** 오늘 세션에서 batching **끔은 14회 중 0회 정상**,
**켬은 7회 중 6회 정상**(프레임 803 · 1,378 · 1,381 · 1,385 · 1,396 · 1,425)입니다.
Tasks 404~413이 좁혀 온 축이 여기서 **사용자가 보는 변화**로 바뀌었습니다.

## 2. 무엇을 바꿨나

`IN`을 평소대로 emulate한 뒤 루프 모양이 맞으면 **카운터 레지스터 하나만** 마지막
반복 직전 값으로 전진시킵니다. EIP·EFLAGS·EAX·EDX는 건드리지 않으므로 게스트가
**마지막 반복을 직접 실행**하고, 종료 상태는 원래 실행과 같습니다.

| 파일 | 변경 |
|---|---|
| `src/platform/win32/io/port_io_delay_loop.h/.cpp` (신규) | 패턴 matcher, 통계, `REPIU_PORT_IO_DELAY_LOOP` 스위치 |
| `src/platform/win32/io/port_io_emulator.cpp` | JAMMA 입력 경로에서만 batching 시도 |
| `src/host/win32/main.cpp`, `CMakeLists.txt` | 통계 로그, 소스 추가 |

**`thread_context.h`를 건드리지 않아** 증분 빌드로 끝났습니다(전체 재빌드는 40분 이상).

## 3. 기계 확인 — 의도한 대로 동작합니다

| 지표 | 값 |
|---|---|
| batch 수 / 건너뛴 반복 | 6,142~11,518회 / **1.2M~2.3M 반복** |
| 1회 최대 건너뜀 | **198** (루프가 200회이므로 2회만 실행) |
| 마지막 루프 본문 / 한계 | `0x0301DB1F` / **200** — Task 405가 지목한 그 루프 |
| `0x0301DB22`의 fault | **97,992~143,337 → 12,284~23,036** |
| tick 전달률 | 47.6~88.3% → **57.8~91.1%** |
| 불일치 사유 | 전부 `shape`(15,788) 또는 `nothing`(7,011). **`not-dead` 0건, `register` 0건, `unreadable` 0건** |

`not-dead` 0건은 **읽은 값이 죽었다는 증명이 매번 성립**했다는 뜻입니다. 증명이 안
되면 batching을 거부하도록 설계했고, 실제로 거부할 일이 없었습니다.

## 4. A/B (같은 세션 · census 끔 · EEPROM 실행별 격리 · 60초)

| run | 조건 | 판정 | frames | quarantine | single-step | 지연 루프 fault |
|---:|---|---|---:|---:|---:|---:|
| 1 | off | 멈춤 | 0 | 1 | 495,444 | 143,337 |
| 2 | on | **멈춤** | 0 | 1 | 448,749 | 18,267 |
| 3 | off | 멈춤 | 0 | 1 | 408,040 | 122,853 |
| 4 | on | 정상 | **803** | 0 | 295 | 12,284 |
| 5 | off | 멈춤 | 0 | 1 | 411,857 | 97,992 |
| 6 | on | 정상 | **1,378** | 1 | 16,550 | 23,036 |

보강 4회(전부 on): **1,425 / 1,381 / 1,385 / 1,396 프레임**, quarantine 1, single-step
14,190~17,845.

**회귀 없음 — pumpit1:** off 2,865 프레임 대 on 2,735 프레임(-4.5%). on 실행의
**batch는 0회**이고 시도만 24,176회이므로, 이 타이틀에는 패턴이 없어 **경로가 아예
동작하지 않습니다.** (오늘 첫 pumpit1 보정 실행은 410 프레임이었는데 이후 2,865까지
올랐습니다. CHD 콜드 캐시 영향으로 보이며, **세션 안에서도 첫 실행은 기준선으로 쓰지
않는 편이 낫습니다.**)

## 5. 사전 등록 기준 대조

설계 §6의 첫 갈래가 성립했습니다 — **fault가 약 10배 줄고 프레임 ≥ 100**. 따라서
**멈춤의 원인은 포화였습니다.** 항목 0b·0c는 폐기하지 않되 우선순위를 내립니다.

**남은 멈춤 1회(run-02)** 는 다른 기전입니다: batching이 정상 동작했는데도
(9,187 batch / 1.8M 반복) single-step이 **448,749**였습니다. 격리 페이지의 single-step
storm이며, 정상 실행 6회의 single-step은 295~17,845입니다. **이제 이것이 남은 최대
원인**이고, frontier 항목 4·5(격리 발생 조건)와 같은 축입니다.

## 6. 회고 — 왜 이번에는 프레임으로 바뀌었나

* **정적 근거가 먼저 있었습니다.** "읽은 값이 쓰이지 않는다"는 것을 디스어셈블로
  확정한 뒤에 손댔습니다. Task 413은 그 반대로, 비용 추정만 가지고 손댔다가 **예산을
  넘는 산술**을 알아채지 못했습니다.
* **가장 싼 수정이 가장 컸습니다.** 레지스터 하나를 앞당기는 것이 전부이고, 플래그를
  합성하지 않았기 때문에 정확성 논증도 필요 없었습니다.
* **거부 조건을 먼저 설계했습니다.** `not-dead`·`register`·`unreadable`이 0건인 것은
  운이 아니라, 증명되지 않으면 아무것도 하지 않도록 짰기 때문입니다.

---

# Task 414 Work Log — batching the port I/O delay loop (**the stall is gone**)

## 1. Result in one line

**pumpit3 renders again.** With batching **off, zero of fourteen** runs today were healthy;
**on, six of seven** were (803, 1,378, 1,381, 1,385, 1,396, and 1,425 frames). The axis
Tasks 404-413 kept narrowing finally became a change the user can see.

## 2. The change

After the `IN` is emulated as usual, a matching loop shape advances **one register** — the
counter — to one iteration before the end. EIP, EFLAGS, EAX, and EDX are untouched, so the
guest runs its **final iteration itself** and the exit state matches an unbatched run. The
matcher and its statistics live in new files under `io/`, the call site is the JAMMA input
path only, and `thread_context.h` was deliberately not touched, so this built incrementally
rather than costing a forty-minute rebuild.

## 3. The mechanism does what it was designed to do

Runs batched 6,142-11,518 loops, skipping **1.2 M to 2.3 M iterations**, with a maximum of
**198** skipped per batch — a 200-iteration loop executed twice. The last matched body is
`0x0301DB1F` with limit **200**, exactly the loop Task 405 named. Faults at `0x0301DB22`
fell from 97,992-143,337 to 12,284-23,036, and tick delivery rose from 47.6-88.3% to
57.8-91.1%. Every refusal was `shape` (15,788) or `nothing to skip` (7,011): **zero
`not-dead`, zero `register`, zero `unreadable`**, meaning the proof that the skipped reads
are dead held on every batch — the design refuses to batch without it.

## 4. A/B (one session, census off, EEPROM isolated per run, 60 s)

Off: 0, 0, 0 frames with 408,040-495,444 single steps and 97,992-143,337 delay-loop faults.
On: 0, 803, 1,378 frames, plus four more on-runs at 1,425, 1,381, 1,385, and 1,396.

**No regression on pumpit1:** 2,865 frames off against 2,735 on (-4.5%), with **zero
batches** in 24,176 attempts — the pattern does not exist in that title, so the path never
engages. (Today's first pumpit1 calibration measured 410 frames before later runs reached
2,865, which looks like a cold CHD cache: **even within a session, the first run should not
be used as a baseline.**)

## 5. Against the pre-registered reading

The design's first branch holds — faults fell about tenfold and frames passed 100 — so **the
stall was saturation.** Frontier items 0b and 0c are not discarded but drop in priority.

**The one remaining stalled run** has a different mechanism: batching worked there (9,187
batches, 1.8 M iterations) yet single steps reached **448,749**, against 295-17,845 in the
six healthy runs. That is the quarantined page's single-step storm, which is now **the
largest remaining cause** and the same axis as frontier items 4 and 5.

## 6. Retrospective — why this one converted to frames

The static evidence came first: the disassembly proved the read values are discarded before
anything was changed. Task 413 did the opposite, acting on a cost estimate, and missed that
its own arithmetic exceeded the whole run's budget. The cheapest change was also the
largest: advancing one register, with no synthesised flags and therefore no correctness
argument to make. And the refusal conditions were designed first — zero `not-dead`,
`register`, and `unreadable` outcomes are not luck but the result of doing nothing whenever
the proof is missing.
