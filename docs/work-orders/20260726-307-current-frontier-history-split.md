# 20260726-307 작업 지시: current execution frontier 이력 분리 / Work order: split current execution frontier history

설계: [20260726-307-current-frontier-history-split.md](../design/20260726-307-current-frontier-history-split.md)

## 한국어

- [x] 기존 frontier 3,657줄을 history 아래에 원문 보존합니다.
- [x] current frontier를 최근 Task 297~306과 현재 결정 중심으로 재작성합니다.
- [x] current/history 문서 간 링크와 analysis 색인을 갱신합니다.
- [x] 원문 archive의 줄 수와 내용 보존을 검증합니다.
- [x] Markdown 링크와 `git diff --check`를 검증하고 작업 로그를 남깁니다.

## English

Preserve the complete 3,657-line frontier under history, rewrite the current document around
Tasks 297-306 and the active architecture decision, update current/history indexes, verify
archive preservation and Markdown links, then record the result in a work log.
