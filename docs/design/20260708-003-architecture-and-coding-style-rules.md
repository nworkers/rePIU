# 아키텍처 및 코딩 스타일 규칙 설계

## 배경

프로젝트가 코드 구현 단계로 들어가면 실제 코드 구조와 설계 결정이 작업 문서에만 흩어질 수 있다.

이를 방지하기 위해 현재 구현되는 코드의 설계와 구조를 `ARCHITECTURE.md`에 지속적으로 누적한다.

## 설계

`ARCHITECTURE.md`는 저장소 루트에 두고, 현재 구현 상태, 예정 디렉터리 구조, 주요 모듈 경계, 갱신 규칙을 기록한다.

코딩 스타일은 `docs/CODING_STYLE.md`에서 관리한다. 기본 기준은 Google C++ Style Guide이며, 프로젝트 예외로 다음 줄 중괄호, 공백 4칸 들여쓰기, 탭 금지, `PascalCase` 함수명, `snake_case` 변수명, `snake_case_` 멤버 변수명, `.cpp`/`.h` 확장자를 명시한다.

`AGENTS.md`에는 `ARCHITECTURE.md` 갱신 의무와 참조 문서 목록을 반영한다.

## Background

Once the project enters code implementation, actual code structure and design decisions can become scattered across task documents.

To prevent that, the design and structure of currently implemented code are continuously accumulated in `ARCHITECTURE.md`.

## Design

`ARCHITECTURE.md` lives at the repository root and records the current implementation state, planned directory structure, major module boundaries, and update rules.

Coding style is maintained in `docs/CODING_STYLE.md`. The baseline is the Google C++ Style Guide, with project exceptions for next-line braces, four-space indentation, no tabs, `PascalCase` function names, `snake_case` variable names, `snake_case_` member variable names, and `.cpp`/`.h` extensions.

`AGENTS.md` records the obligation to update `ARCHITECTURE.md` and includes it in the reference document list.
