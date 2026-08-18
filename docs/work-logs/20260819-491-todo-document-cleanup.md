# Task 491 TODO 문서 정리 작업 로그

설계: [20260819-491-todo-document-cleanup.md](../design/20260819-491-todo-document-cleanup.md)

작업 지시: [20260819-491-todo-document-cleanup.md](../work-orders/20260819-491-todo-document-cleanup.md)

## 결과

- `docs/TODO.md`를 7월 bootstrap 진행 일지와 중복 완료 목록에서 실제 보류 항목
  인덱스로 되돌렸습니다.
- Task 365에서 남긴 setter/batching 후속은 Tasks 439·443·444의 구현·검증·기본값
  승격 근거와 대조해 완료로 정리했습니다.
- `0x02A0` 계열 의미 보류는 Task 290이 YMZ280B register/data byte lane으로 확정하고
  HLE에 연결한 근거로 완료 처리했습니다.
- 2026-07-09~10의 opcode, segment, DOS file/current-directory/IOCTL, runtime arena
  “다음 작업” 기록은 후속 작업 로그에 이미 이어졌으므로 전역 TODO에서 제거했습니다.
- Task 490에서 완료한 stall watchdog 항목도 완료 근거 링크로 남겼습니다.
- 현재 명시적으로 보류된 활성 TODO가 없음을 날짜와 함께 표시했습니다.
- 향후 임시 관측점은 work log/analysis에, 여러 작업에 걸쳐 의도적으로 보류하는 항목만
  전역 TODO에 기록하도록 역할을 명확히 했습니다.

## 검증

- TODO와 Task 491 설계·작업 지시서의 Markdown 상대 링크를 추출해 모두 실제 파일로
  해석되는 것을 확인했습니다.
- 제거 대상 heading과 `Remaining Real Implementation Work` 문구가 TODO에 남지 않은
  것을 `rg`로 확인했습니다.
- `git diff --check`: 통과했습니다.
- 문서 전용 변경이므로 코드 빌드는 수행하지 않았습니다.

---

# Task 491 TODO Document Cleanup Work Log

Design: [20260819-491-todo-document-cleanup.md](../design/20260819-491-todo-document-cleanup.md)

Work order: [20260819-491-todo-document-cleanup.md](../work-orders/20260819-491-todo-document-cleanup.md)

## Result

- Returned `docs/TODO.md` from a duplicated July bootstrap journal and completion list
  to an index of genuinely deferred work.
- Closed Task 365's setter/batching follow-ups against the implementation,
  verification, and default-promotion evidence from Tasks 439, 443, and 444.
- Closed the deferred meaning of the `0x02A0` family against Task 290, which identified
  YMZ280B register/data byte lanes and connected them to HLE.
- Removed the July 9-10 opcode, segment, DOS file/current-directory/IOCTL, and runtime
  arena “next task” records because subsequent work logs already continue those chains.
- Retained a closure link for the stall-watchdog issue completed by Task 490.
- Stated with a date that there are currently no explicitly deferred active TODO items.
- Clarified that temporary observation points belong in work logs/analysis, while the
  global TODO contains only intentionally deferred cross-task work.

## Verification

- Extracted relative Markdown links from the TODO and Task 491 design/work order and
  confirmed that every target resolves to an existing file.
- Used `rg` to confirm removed headings and `Remaining Real Implementation Work` no
  longer appear in the TODO.
- `git diff --check` passed.
- No code build was run because this is documentation-only cleanup.
