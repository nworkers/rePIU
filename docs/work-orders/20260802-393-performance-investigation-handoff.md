# 20260802-393 성능 조사 인계 및 병합 작업 지시 / Performance Investigation Handoff and Merge Work Order

설계: [20260802-393-performance-investigation-handoff.md](../design/20260802-393-performance-investigation-handoff.md)

## 한국어

1. Task 377~392의 채택·기각 결과와 남은 확인 항목을 설계와 현재 실행 frontier에 정리합니다.
2. 최종 Release loader와 AOT probe를 빌드합니다.
3. 두 PIU 실행 파일 구성에서 전체 AOT probe를 실행합니다.
4. `VERSION`을 0.0.122로 올리고 작업 로그를 작성합니다.
5. 작업 브랜치 변경을 커밋한 뒤 `main`에 squash 병합합니다.
6. `v0.0.122` annotated tag를 로컬에 만들고 작업 브랜치를 삭제합니다.

## English

1. Summarize adopted/rejected Tasks 377-392 and remaining checks in the design and current execution frontier.
2. Build the final Release loader and AOT probe.
3. Run the full AOT probe against both PIU executable layouts.
4. Bump `VERSION` to 0.0.122 and write the work log.
5. Commit the task-branch changes and squash-merge them into `main`.
6. Create local annotated tag `v0.0.122` and delete the task branch.
