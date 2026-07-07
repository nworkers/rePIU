# 프로젝트 작업 규칙 설계

## 배경

프로젝트는 원본 DOS/4G 실행 파일을 보존 실행하는 장기 작업이므로, 코드 변경보다 설계와 추적 가능성이 먼저 정리되어야 한다.

## 설계

작업 흐름은 요구사항 접수, 맥락 확인, 설계 문서 작성 또는 갱신, 작업 지시 작성 또는 갱신, 구현, 검증, 작업 로그 작성 순서로 고정한다.

문서는 한국어를 먼저 쓰고 영어 번역을 바로 아래에 추가한다. 실행 파일 분석 결과는 한국어/영어 문서를 분리하여 누적한다.

플랫폼 공용 구조를 먼저 설계하고, Win32/Linux/Web 세부 구현은 플랫폼별 디렉터리로 분리한다.

## Background

The project is a long-running effort to preserve and execute the original DOS/4G executable, so design clarity and traceability must come before code changes.

## Design

The workflow is fixed as requirement intake, context inspection, design document creation or update, work-order creation or update, implementation, verification, and work-log creation.

Documents are written in Korean first, followed immediately by English translation. Executable analysis findings are accumulated in separate Korean and English documents.

Shared multiplatform structures are designed first, and Win32/Linux/Web details are separated into platform-specific directories.
