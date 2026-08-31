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
  [Linux 이식 frontier](analysis/linux-port-frontier.md)의 **3.9 세션 인수인계**가
  정본이고, 실행 모델은 [Task 546 설계](design/20260831-546-linux-x64-aot-dbt-execution-model.md)
  입니다. Tasks 549–557은 **`v0.0.175`로 main에 머지됐습니다**.
  **지금까지:** x64 host가 실제 fault를 받아 재개하고, `int3` rewind와 trap flag
  single-step이 동작하며, 다른 thread의 register를 편집합니다. guest arena가 하위
  4 GiB에 배치되는 것을 측정했고(결정 4 확정), 32비트 바이트를 복사해도 되는지 판정하는
  fail-closed 판정기와 memory operand lowering이 실행으로 검증됐습니다.
  **Task 553이 그 lowering을 code cache emitter에 연결했습니다(구현 순서 3단계 완료).**
  경계는 `enable_long_mode_emission` build option이고 기본값이 `false`라 **i386 경로는
  그 판단 branch를 지나가지 않습니다**. 켜면 `kCopy`만 방출되고 나머지 kind는 전부
  fail-closed입니다. 같은 작업에서 방출 후 검증기가 "조용히 다른 명령"을 놓치고 있던 것을
  찾아 고쳤습니다 — 길이 합계만 보고 있었습니다.
  **Task 554가 step 4 앞의 막힌 곳을 찾아 열었습니다: x64 host는 code cache를 하나도
  배치하지 못하고 있었습니다.** cache 주소가 host pointer인데 32비트 필드에 담기고,
  hint 없는 요청은 `0x00007fdd…`로 떨어져 거절됐습니다. 넓히는 대신(참조 121곳, 그리고
  방출된 `disp32`는 넓어지지 않음) 64비트 host에서만 하위 4 GiB 후보 사다리를 타게 했고,
  이제 `0x20000000`에 배치됩니다.
  **Task 555가 lowering의 결함 하나를 닫았습니다:** `add esp,16`이 `kIdenticalBytes`로
  cache에 그대로 복사되고 있었고, long mode에서 그것은 host stack pointer를 파괴합니다.
  근인은 적히지 않은 전제("lowering 시점에 guest GPR n이 host GPR n에 있다")였고 이제
  헤더에 적혀 있습니다.
  **Task 556이 남은 거리를 숫자로 만들었습니다: 명령의 51.15%, 그러나 완결 block은
  1.82%(260/14,307)입니다.** block은 control flow로 끝나므로 기대 연쇄 길이가 약 1
  block이고, **명령의 51%는 실행의 51%가 아닙니다.** 거절은 stack 계열 14,618(50%)과
  control flow 12,856(44%)이 거의 같은 크기이며 **둘 다 있어야 연쇄가 생깁니다.**
  **Task 557이 싼 항목을 처리했습니다:** `INC`/`DEC r32` 784건이 `FF /0`·`FF /1`로
  낮춰져 방출 가능이 52.46%, 완결 block이 316(2.21%)이 됐고, 가장 위험한 부류인
  `silently-different`가 1,466 → 682로 줄었습니다. 같은 측정이 **Task 556의 서술 하나를
  정정했습니다** — `operand-width`는 사실상 전부 `push`·`pop`이고 **x87은 거기 없습니다.**
  x87 거절은 전부 `[esp]` 때문이며 2,758건 중 약 1,900건은 이미 방출됩니다.
  **다음 단위:** (1) stack 명령 lowering + guest ESP를 state에서 꺼내기, (2) x64 dispatch
  slot 방출·thunk·resolver. 두 개가 한 묶음이고, x87 대부분이 (1)에 딸려 옵니다.
  **guest 실행 자체는 Task 544의 fail-closed 그대로입니다.**

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
