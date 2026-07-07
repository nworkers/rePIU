# 정적 HLE Profile 설계

## 배경

`TargetProfile`은 `hle_profile_id`를 가지지만, 아직 이 값을 해석하는 HLE profile registry가 없다.

프로젝트는 모든 DOS 실행 파일이 아니라 DOS/4GW 계열 실행 파일을 대상으로 하며, 게임별 차이는 target profile과 HLE profile로 분리한다.

초기 단계에서는 동적 플러그인 시스템을 만들지 않고, C++ 코드에 정적으로 등록되는 HLE profile을 사용한다.

## 목표

이번 단계는 HLE 구현 전, target이 요구하는 HLE 서비스 범위를 표현하는 최소 구조를 추가한다.

* `HleProfile` 구조를 추가한다.
* HLE 서비스 id와 이름 변환 함수를 추가한다.
* `piu_common` HLE profile을 정적으로 등록한다.
* `repiu_exe_analyzer`가 target profile의 `hle_profile_id`를 조회하고 서비스 목록을 출력한다.

## 비목표

* DOS/DPMI/HW HLE 실제 구현
* HLE hook/override 호출
* 동적 DLL 플러그인 로딩
* 원본 코드 실행

## 설계

`HleProfile`은 다음 정보를 가진다.

* profile id
* 표시 이름
* 설명
* 필요한 HLE 서비스 목록

초기 HLE 서비스는 다음 범주를 가진다.

* DOS file
* DOS memory
* DPMI
* timer
* input
* video
* audio

`piu_common`은 PIU 계열 DOS/4GW 실행 파일이 사용할 공통 HLE profile로 등록한다.

## 검증 기준

* Debug 빌드가 성공한다.
* `repiu_exe_analyzer`가 `HLE profile: piu_common`과 HLE profile 이름을 출력한다.
* 출력에 `HLE services:` 목록이 포함된다.
* 기존 실행 파일 분석 결과가 유지된다.

## 향후 확장

다음 단계에서는 실제 trace가 나오기 전까지 HLE profile을 선언적 범위로 유지한다.

원본 코드 실행 단계에서 호출된 interrupt와 I/O에 따라 profile의 서비스 목록과 quirk를 세분화한다.

## Background

`TargetProfile` has an `hle_profile_id`, but there is no HLE profile registry that resolves it yet.

The project targets DOS/4GW-family executables rather than every DOS executable, and game-specific differences are separated into target profiles and HLE profiles.

The initial step uses statically registered HLE profiles in C++ instead of a dynamic plugin system.

## Goal

This step adds the minimum structure for expressing the HLE service scope required by a target before implementing HLE behavior.

* Add the `HleProfile` structure.
* Add HLE service ids and name conversion.
* Register the `piu_common` HLE profile statically.
* Make `repiu_exe_analyzer` resolve the target profile's `hle_profile_id` and print the service list.

## Non-Goals

* Implementing DOS/DPMI/HW HLE behavior.
* Invoking HLE hooks or overrides.
* Dynamic DLL plugin loading.
* Executing original code.

## Design

`HleProfile` contains:

* profile id
* display name
* description
* required HLE service list

The initial HLE services are:

* DOS file
* DOS memory
* DPMI
* timer
* input
* video
* audio

`piu_common` is registered as the shared HLE profile for PIU-family DOS/4GW executables.

## Verification Criteria

* Debug build succeeds.
* `repiu_exe_analyzer` prints `HLE profile: piu_common` and the HLE profile name.
* The output includes the `HLE services:` list.
* Existing executable analysis results are preserved.

## Future Extension

Until real traces are available, HLE profiles remain declarative service scopes.

During the original-code execution phase, the service list and quirks will be refined from observed interrupts and I/O.
