# 프로젝트 헌장

## 목적

rePIU는 원본 DOS/4G 기반 PIU 실행 파일을 가능한 한 그대로 실행하고, DOS/DPMI/하드웨어 환경만 HLE 계층으로 대체하는 네이티브 런타임을 만든다.

게임 로직은 원본 32-bit x86 코드가 담당해야 하며, C++ 코드는 실행 환경과 관찰 가능한 외부 서비스를 제공하는 데 집중한다.

## Purpose

rePIU builds a native runtime that executes the original DOS/4G-based PIU executable as directly as possible while replacing only the DOS, DPMI, and hardware environment with HLE layers.

Game logic must remain in the original 32-bit x86 code. C++ code should focus on the execution environment and observable external services.

## 현재 1차 목표

첫 번째 목표 실행 파일은 `MASTER\PIU_1ST\PIU.EXE`이다.

향후 여러 버전을 지원해야 하므로 실행 파일 경로, 작업 디렉터리, 자산 루트, 버전별 HLE 특성은 타깃 프로파일로 분리한다.

## Current First Target

The first target executable is `MASTER\PIU_1ST\PIU.EXE`.

Because multiple versions must be supported later, executable paths, working directories, asset roots, and version-specific HLE behavior should be separated into target profiles.

## 방향성

* 원본 실행 파일을 권위 있는 구현으로 취급한다.
* DOSBox 또는 전체 PC 에뮬레이터를 통합하지 않는다.
* 실행 파일 분석, 메모리 매핑, DPMI/DOS HLE, 그래픽, 입력, 타이밍, 오디오는 독립 하위 시스템으로 분리한다.
* DOS/4GW 로더 코어는 공용으로 유지하고, 게임/버전별 차이는 정적 target profile과 향후 HLE profile/override로 분리한다.
* HLE profile은 실제 게임 로직을 대체하지 않고, 원본 코드 주변 환경 서비스의 범위를 선언한다.
* 원본 32-bit x86 코드 직접 실행은 Win32 x86 host build를 기준으로 검증한다.
* 플랫폼 공용 코어를 먼저 설계하고 Win32/Linux/Web 세부 구현은 플랫폼 계층에 둔다.
* 코드 변경 전에는 설계와 작업 계획을 문서화한다.
* 프로젝트 버전은 `VERSION` 파일의 `major.minor.patch` 형식으로 관리한다.
* 호환성 테스트 milestone 결과는 dashboard 입력으로 활용할 수 있도록 날짜와 버전이 포함된 JSON history로 누적한다.

## Direction

* Treat the original executable as the authoritative implementation.
* Do not integrate DOSBox or a full PC emulator.
* Keep executable analysis, memory mapping, DPMI/DOS HLE, graphics, input, timing, and audio as independent subsystems.
* Keep the DOS/4GW loader core shared, and separate game/version-specific differences into static target profiles and future HLE profiles/overrides.
* HLE profiles declare the scope of surrounding environment services and do not replace original game logic.
* Direct execution of original 32-bit x86 code is verified against a Win32 x86 host build.
* Design the shared platform-neutral core first and keep Win32/Linux/Web specifics in platform layers.
* Document design and work plans before changing code.
* Manage the project version in the `VERSION` file using `major.minor.patch`.
* Accumulate compatibility-test milestone results as dated and versioned JSON history for future dashboard input.
