# Task 491 TODO 문서 정리 설계

## 배경

`docs/TODO.md`는 보류 항목 목록으로 시작했지만 2026년 7월 9~10일의 실행 blocker,
완료 상태, 당시의 “다음 작업”을 함께 누적했습니다. 이후 구현이 진행된 뒤에도 과거
기록이 남아 실제 미완료 작업처럼 보였습니다.

현재 상단의 두 보류 묶음도 후속 작업으로 닫혔습니다.

- Task 365의 setter/batching 후속은 Tasks 437~444에서 구현·측정·기본값 승격까지
  진행됐습니다.
- `0x02A0` 계열은 Task 290에서 YMZ280B register/data byte lane으로 확인되고 HLE에
  연결됐습니다.
- 7월 9~10일 blocker는 대응 작업 로그와 누적 analysis에 보존되어 있으며 현재
  우선순위가 아닙니다.

## 설계

1. `docs/TODO.md`는 실제로 보류된 미완료 항목만 담는 짧은 문서로 되돌립니다.
2. 현재 활성 항목이 없음을 날짜와 함께 명시합니다.
3. 제거한 두 보류 묶음과 역사적 bootstrap 기록은 대표 완료 근거에 링크합니다.
4. 현재 실행 우선순위는 `docs/analysis/current-execution-frontier.md`, 승인된 구현 단위는
   `docs/work-orders/`, 완료 이력은 `docs/work-logs/`가 소유한다고 안내합니다.
5. 앞으로는 특정 작업 로그의 일시적인 “다음 관측점”을 전역 TODO에 중복하지 않고,
   여러 작업에 걸쳐 명시적으로 보류할 항목만 추가합니다.

## 검증

- TODO 본문에 완료된 blocker나 “남은 실제 구현 작업” 목록이 없는지 확인합니다.
- 모든 Markdown 상대 링크가 실제 파일을 가리키는지 확인합니다.
- 한국어/영어 순서와 내용 대응을 확인합니다.

---

# Task 491 TODO Document Cleanup Design

## Background

`docs/TODO.md` began as a deferred-item list but accumulated July 9-10 execution
blockers, completion status, and then-current “next tasks.” Those historical records
continued to look active after later implementation had superseded them.

The two remaining deferred groups are now closed as well.

- Tasks 437-444 implemented, measured, and promoted the setter/batching follow-ups from
  Task 365.
- Task 290 identified the `0x02A0` family as YMZ280B register/data byte lanes and wired
  it into HLE.
- The July 9-10 blockers remain preserved in their work logs and cumulative analysis;
  they are not current priorities.

## Design

1. Return `docs/TODO.md` to a short list containing only genuinely deferred unfinished
   items.
2. State with a date that there are currently no active entries.
3. Link representative closure evidence for the two deferred groups and the historical
   bootstrap record being removed.
4. Direct current execution priorities to `docs/analysis/current-execution-frontier.md`,
   approved implementation units to `docs/work-orders/`, and completed history to
   `docs/work-logs/`.
5. Do not duplicate a task log's temporary “next observation point” into the global
   TODO. Add only items intentionally deferred across multiple tasks.

## Verification

- Confirm that no completed blockers or “remaining real implementation work” lists
  remain in the TODO body.
- Confirm every relative Markdown link resolves to a real file.
- Confirm Korean-first bilingual ordering and equivalent content.
