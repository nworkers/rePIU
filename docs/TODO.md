# TODO

## 현재 상태

2026-08-31 기준 활성 항목은 둘입니다. **Linux x64 host 이식이 진행 중**이고, 웹 이식
Stage 3~5는 **보류 중**입니다 — 보류 이유는 우선순위이며 아래 항목에 적혀 있습니다.

Task 492~495의 JAMMA 입력 timing, IRQ0 replay, 2P 숫자패드 별칭과 history 정리는 사용자
120초 실행으로 최종 검증됐습니다.

현재 실행 우선순위와 확인된 병목은 [현재 실행 frontier](analysis/current-execution-frontier.md),
승인된 구현 단위는 [`work-orders/`](work-orders/), 완료 결과와 검증 근거는
[`work-logs/`](work-logs/)에서 관리합니다.

## 활성 항목

- **Linux x64 host 이식 — 진행 중(2026-09-01).** 상태와 다음 한 걸음은
  [Linux 이식 frontier](analysis/linux-port-frontier.md)의 **3.10 세션 인수인계**가
  정본입니다. 실행 모델은
  [Task 546 설계](design/20260831-546-linux-x64-aot-dbt-execution-model.md)이고,
  Tasks 549–559는 **`v0.0.176`으로 main에 머지됐습니다**.
  **한 줄로:** x64가 emitter의 바이트를 실제로 실행하고 guest 명령의 **66.17%**를 낼 수
  있지만, **완결 block은 2.66%**입니다 — block이 control flow로 끝나고 그것이 아직 하나도
  방출되지 않기 때문입니다. **명령의 2/3는 실행의 2/3가 아닙니다.**
  **다음 단위:** control flow — `CALL`/`RET`/`JMP`와 x64 dispatch slot·thunk·resolver
  (`not-a-copy-record` 12,856, Task 546 구현 순서 4단계). **이것 없이는 완결 block이 늘지
  않습니다.** 그 뒤가 `ESP`를 피연산자로 쓰는 일반 명령 재인코더(`stack-pointer` 6,401,
  x87 대부분 포함), 그리고 5단계 block 단위 대조입니다.
  **guest 실행 자체는 Task 544의 fail-closed 그대로입니다** — x64가 실행한 것은 probe가
  만든 프로그램이지 게임이 아닙니다.

- **웹(WebAssembly) 이식 Stage 3~5 — 보류(2026-08-28).** 상태와 재개 조건은
  [웹 이식 frontier](analysis/web-port-frontier.md)가 정본입니다. **보류 이유는 이 작업의
  문제가 아니라 우선순위입니다** — Linux는 게임이 이미 화면까지 나오고 성능 축이 열려 있는데,
  웹은 아직 실행 backend가 없습니다.
  Task 513 Stage 1이 플랫폼 공용 코어를 wasm32로
  빌드했고, Task 514 Stage 2가 크기를 쟀습니다 — **명령 형태 320개, 상위 17 mnemonic이
  87.89%.** 브라우저에서 게임은 아직 실행되지 않습니다. 현재 실행 backend 둘(`legacy`,
  `dynamic`)이 모두 네이티브 x86 위에 서 있어 wasm에 형태가 없기 때문입니다.
  다섯 단계는 [웹 실행 설계](design/20260828-513-web-wasm-execution.md)에 있습니다.
  **재개 조건:** 다음 단위는 Stage 3(플랫폼 중립 인터프리터)이고, 상위 17개가 87.89%이므로
  점진적으로 세울 수 있습니다. **단 x87 표현은 동적 census 뒤에 정합니다** — 80비트가
  관측 가능한 것은 확인됐고(14곳), 레지스터 파일 전체가 메모리로 노출되는지는 미확정입니다
  ([Task 514 로그](work-logs/20260828-514-guest-instruction-census.md)의 x87 절).
  80비트 레지스터 파일을 나중에 바꾸는 것은 인터프리터를 다시 쓰는 일입니다.
  **그리고 Stage 3은 Worker 실행을 전제로 설계합니다** — CHD가 플레이 내내 열려 있어야 하고
  브라우저에서 동기 파일 I/O는 Worker 안에서만 성립하기 때문입니다(설계 513 결정 7).

## 이번 정리에서 닫힌 이전 항목

- **Task 492 최초 live 검증:** 입력 edge와 due queue에는 유실이 없었지만 IF gate 때문에
  replay가 소비되지 않는 사실을 확인했고 Task 493에서 수정했습니다.
- **Task 494 2P 숫자패드 재검증:** 사용자 로그에서 다섯 위치가 모두 균형 잡힌
  press/release transition을 기록해 keycode 별칭 수정을 확인했습니다.
- **Task 495 JAMMA history 재검증:** 120초 사용자 실행에서 1,006 edge를 모두 안전하게
  정리했고 `history-peak=4`, 모든 timeline loss counter 0을 확인했습니다.
- **Task 365 Glide setter/batching 후속:** Task 439, 443, 444에서 구현·검증·기본값 승격을
  완료했습니다.
- **Port I/O `0x02A0` 계열 의미:** Task 290에서 YMZ280B register/data byte lane으로
  확인하고 sound HLE에 연결했습니다.
- **Task 445에서 발견된 stall watchdog 오판:** Task 490에서 wall timeout과 stall timeout을
  분리하고 Glide direct dispatch를 진행 신호에 포함해 완료했습니다.

## 항목 추가 기준

특정 작업의 일시적인 다음 관측점은 해당 work log나 analysis에 기록합니다. 여러 작업에
걸쳐 의도적으로 보류하며 별도의 재개 조건이 있는 미완료 항목만 이 문서에 추가합니다.

---

# TODO

## Current Status

As of 2026-08-28 there is one item, Stages 3 through 5 of the web port, and it is **on hold** --
the Linux performance axis comes first. A 120-second user run
completed final validation
of Tasks 492 through 495: JAMMA input timing, IRQ0 replay, 2P numpad aliases, and history pruning.

Current execution priorities and confirmed bottlenecks live in the
[current execution frontier](analysis/current-execution-frontier.md), approved implementation
units in [`work-orders/`](work-orders/), and completed results and verification evidence in
[`work-logs/`](work-logs/).

## Active Item

- **Web (WebAssembly) port, Stages 3 through 5 -- on hold (2026-08-28).** The state and the resume
  conditions live in the [web port frontier](analysis/web-port-frontier.md). **It is on hold for
  priority, not because anything is wrong with it**: on Linux the game already reaches the screen and
  the performance axis is open, while the web has no execution backend yet.
  Task 513 Stage 1 built the platform-neutral core
  for wasm32, and Task 514 Stage 2 measured the size: **320 instruction forms, with the top 17
  mnemonics covering 87.89%.** The game still does not run in a browser: both current execution
  backends (`legacy` and `dynamic`) stand on native x86, which has no wasm form. The five stages are
  in the [web execution design](design/20260828-513-web-wasm-execution.md).
  **Resume condition:** the next unit is Stage 3, the platform-neutral interpreter, and the top 17
  covering 87.89% means it can be built incrementally. **But settle the x87 representation after a
  dynamic census**: 80 bits are confirmed observable (14 sites), while whether the whole register
  file is exposed to memory is unresolved (see the x87 section of the
  [Task 514 log](work-logs/20260828-514-guest-instruction-census.md)). Changing the 80-bit register
  file later means writing the interpreter twice.
  **And design Stage 3 for running in a Worker**: the CHD has to stay open throughout play, and
  synchronous file I/O in a browser holds only inside a Worker (design 513, Decision 7).

## Previous Entries Closed by This Cleanup

- **Initial Task 492 live validation:** it confirmed no edge or due-queue loss but found that the IF
  gate prevented replay consumption; Task 493 corrected it.
- **Task 494 2P numpad revalidation:** the user log recorded balanced press/release transitions for
  all five positions, confirming the keycode-alias correction.
- **Task 495 JAMMA history revalidation:** a 120-second user run safely pruned all 1,006 edges,
  peaked at four retained entries, and reported zero for every timeline loss counter.
- **Task 365 Glide setter/batching follow-ups:** Tasks 439, 443, and 444 completed implementation,
  verification, and default promotion.
- **Meaning of the `0x02A0` Port-I/O family:** Task 290 identified YMZ280B register/data byte lanes
  and connected them to sound HLE.
- **The stall-watchdog false positive found in Task 445:** Task 490 separated wall and stall
  timeouts and added Glide direct dispatch to the progress definition.

## Entry Criteria

Keep a task's temporary next observation point in its work log or cumulative analysis. Add an item
here only when unfinished work is intentionally deferred across tasks and has an explicit condition
for resuming it.
