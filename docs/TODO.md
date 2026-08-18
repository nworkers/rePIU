# TODO

## 현재 상태

2026-08-19 기준 활성 TODO는 없습니다. Task 492~495의 JAMMA 입력 timing, IRQ0 replay,
2P 숫자패드 별칭과 history 정리는 사용자 120초 실행으로 최종 검증됐습니다.

현재 실행 우선순위와 확인된 병목은 [현재 실행 frontier](analysis/current-execution-frontier.md),
승인된 구현 단위는 [`work-orders/`](work-orders/), 완료 결과와 검증 근거는
[`work-logs/`](work-logs/)에서 관리합니다.

## 활성 항목

없습니다.

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

As of 2026-08-19, there are no active TODO items. A 120-second user run completed final validation
of Tasks 492 through 495: JAMMA input timing, IRQ0 replay, 2P numpad aliases, and history pruning.

Current execution priorities and confirmed bottlenecks live in the
[current execution frontier](analysis/current-execution-frontier.md), approved implementation
units in [`work-orders/`](work-orders/), and completed results and verification evidence in
[`work-logs/`](work-logs/).

## Active Item

None.

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
