# 정적 TargetProfile 설계

## 배경

프로젝트 범위는 모든 DOS 실행 파일이 아니라 DOS/4GW 계열 실행 파일을 대상으로 한다.

로더 코어는 DOS/4GW 계열 Watcom/LE 실행 파일을 공통으로 처리하고, 게임이나 버전별 차이는 프로파일로 분리한다.

초기 단계에서는 동적 DLL 플러그인 시스템을 만들지 않고, C++ 코드에 정적으로 등록되는 `TargetProfile`과 `TargetRegistry`를 사용한다.

## 목표

이번 단계는 여러 버전과 게임별 HLE 확장을 받을 수 있는 최소 공용 구조를 만든다.

* `TargetProfile` 구조를 추가한다.
* `TargetRegistry` 조회 함수를 추가한다.
* `piu_1st` 기본 프로파일을 정적으로 등록한다.
* 분석 도구의 기본 실행 파일 경로를 `piu_1st` 프로파일에서 가져오게 한다.

## 비목표

* 동적 DLL 플러그인 로딩
* 게임별 HLE override 구현
* DOS/4GW 로더 클래스 리팩터링
* 원본 코드 실행

## 설계

`TargetProfile`은 다음 정보를 가진다.

* target id
* 표시 이름
* 실행 파일 경로
* 작업 디렉터리
* 자산 루트
* 실행 파일 포맷 힌트
* HLE 프로파일 id

`TargetRegistry`는 내장 프로파일 목록을 반환하고, id로 프로파일을 조회한다.

초기 등록 대상은 `piu_1st` 하나이다.

## 검증 기준

* Debug 빌드가 성공한다.
* `repiu_exe_analyzer`를 인자 없이 실행했을 때 `piu_1st` 프로파일의 실행 파일 경로를 사용한다.
* 명시적 경로 인자를 넣었을 때 기존처럼 해당 경로를 분석한다.

## 향후 확장

다음 단계에서 HLE 프로파일과 target별 known quirk를 별도 구조로 확장한다.

동적 플러그인은 여러 게임/외부 확장이 실제로 필요해진 뒤 별도 설계한다.

## Background

The project targets DOS/4GW-family executables, not every DOS executable.

The loader core should handle DOS/4GW-family Watcom/LE executables through a shared path, while game- or version-specific differences are separated into profiles.

The initial step does not add a dynamic DLL plugin system. It uses `TargetProfile` and `TargetRegistry` registered statically in C++.

## Goal

This step adds the minimum shared structure that can later receive multiple versions and game-specific HLE extensions.

* Add the `TargetProfile` structure.
* Add `TargetRegistry` lookup functions.
* Register the default `piu_1st` profile statically.
* Make the analysis tool obtain its default executable path from the `piu_1st` profile.

## Non-Goals

* Dynamic DLL plugin loading.
* Game-specific HLE override implementation.
* Refactoring into a DOS/4GW loader class.
* Executing original code.

## Design

`TargetProfile` contains:

* target id
* display name
* executable path
* working directory
* asset root
* executable format hint
* HLE profile id

`TargetRegistry` returns the built-in profile list and can look up a profile by id.

The initial registered target is `piu_1st`.

## Verification Criteria

* Debug build succeeds.
* Running `repiu_exe_analyzer` without arguments uses the executable path from the `piu_1st` profile.
* Running it with an explicit path still analyzes that path as before.

## Future Extension

A later step will extend HLE profiles and target-specific known quirks as separate structures.

Dynamic plugins should be designed separately only after multiple games or external extensions actually need them.
