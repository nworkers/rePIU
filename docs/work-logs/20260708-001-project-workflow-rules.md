# 작업 로그: 프로젝트 작업 규칙 정리

## 결과

완료.

`AGENTS.md`에 사용자가 제공한 핵심 원칙, 응답 태도, 요구사항 처리 절차, 문서 작성 규칙, 구현 규칙, 작업 단위 규칙, Git 작업 규칙을 반영했다.

관련 기준 문서로 `docs/PROJECT_CHARTER.md`, `docs/CODING_STYLE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`를 추가했다.

새 문서 디렉터리 규칙에 맞춰 `docs/design/`, `docs/work-orders/`, `docs/work-logs/` 아래에 이번 작업 문서를 추가했다.

기존 `docs/PIU_EXE_LOADER_WORK_PLAN.md`에는 새 기준 작업 지시 문서 위치 안내를 추가하고, `docs/work-orders/20260708-002-piu-exe-loader.md`를 추가했다.

## 검증

`Get-ChildItem docs -Recurse -File`로 문서 파일 생성 여부를 확인했다.

`git branch --show-current`로 현재 브랜치가 `docs/add-project-workflow-rules`임을 확인했다.

`git status --short --branch`로 변경 상태를 확인했다. 현재 저장소는 초기 커밋 전 상태라 `AGENTS.md`, `docs/`, `MASTER/`가 모두 untracked로 표시된다.

## Work Log: Project Workflow Rules

## Result

Complete.

Updated `AGENTS.md` with the user's core principles, response tone, requirement handling procedure, documentation rules, implementation rules, task unit rules, and Git workflow rules.

Added related baseline documents: `docs/PROJECT_CHARTER.md`, `docs/CODING_STYLE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md`.

Added task documents under `docs/design/`, `docs/work-orders/`, and `docs/work-logs/` according to the new documentation directory rules.

Added a moved notice to the old `docs/PIU_EXE_LOADER_WORK_PLAN.md` and added `docs/work-orders/20260708-002-piu-exe-loader.md` as the canonical work-order document.

## Verification

Confirmed document creation with `Get-ChildItem docs -Recurse -File`.

Confirmed the current branch is `docs/add-project-workflow-rules` with `git branch --show-current`.

Checked the worktree with `git status --short --branch`. Because the repository has no initial commit yet, `AGENTS.md`, `docs/`, and `MASTER/` are all shown as untracked.
