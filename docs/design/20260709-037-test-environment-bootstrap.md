# 테스트 환경 부트스트랩 설계

## 배경

현재 구현 검증은 Win32 x86 host 빌드, OpenWatcom DOS/4GW 샘플 빌드, `dos4gw_hello` 실행, `piu_1st` 실행으로 나뉜다.
다른 PC에서 같은 검증을 수행하려면 CMake, Visual Studio 2022 Win32 도구체인, OpenWatcom, 원본 `PIU.EXE` 배치 상태를 빠르게 확인할 수 있어야 한다.

기존 구현 원칙상 원본 게임 코드는 주 실행 경로로 유지하며, 이번 작업은 실행 환경 준비와 검증 절차 자동화만 다룬다.

## 목표

* 로컬 PC에 필요한 테스트 전제 조건을 점검하는 스크립트를 둔다.
* Visual Studio에 포함된 CMake가 있으면 PATH에 없어도 사용할 수 있게 한다.
* OpenWatcom은 기존 `scripts/install_openwatcom.ps1`를 통해 `tools/openwatcom/`에 설치한다.
* 샘플 DOS/4GW executable을 생성하고, 현재 구현된 loader target을 모두 실행하는 검증 스크립트를 둔다.
* 실패 시 어떤 도구나 자산이 빠졌는지 명확한 메시지를 남긴다.

## 비목표

* Visual Studio, CMake, Git을 패키지 매니저로 자동 설치하지 않는다.
* OpenWatcom 바이너리나 다운로드 파일을 Git에 커밋하지 않는다.
* loader, HLE, 원본 실행 코드 경로를 변경하지 않는다.

## 설계

`scripts/setup_test_environment.ps1`는 다음을 수행한다.

* Git, CMake, Visual Studio Win32 도구체인, 원본 `MASTER/PIU_1ST/PIU.EXE` 존재 여부를 확인한다.
* CMake가 PATH에 없으면 `vswhere.exe`로 Visual Studio 번들 CMake를 찾고 현재 프로세스 PATH에 추가한다.
* OpenWatcom이 없으면 기존 설치 스크립트를 호출한다.
* OpenWatcom 설치 후 `scripts/build_dos4gw_hello.bat`를 실행해 샘플 executable을 준비한다.

`scripts/test_all.ps1`는 다음을 수행한다.

* `setup_test_environment.ps1`를 호출해 누락된 로컬 테스트 환경을 먼저 준비한다.
* `scripts/build_win32_x86.bat`로 Win32 x86 Debug 빌드를 수행한다.
  이 배치 파일은 `scripts/build_win32_x86.ps1`를 호출하며, 설치된 Visual Studio 버전에 맞춰 `Visual Studio 18 2026`, `Visual Studio 17 2022`, `Visual Studio 16 2019` generator 중 하나를 선택한다.
* `scripts/build_dos4gw_hello.bat`로 DOS/4GW 샘플을 다시 빌드한다.
* `repiu_loader_win32.exe dos4gw_hello`를 실행하고 `Hello, world!` 출력과 종료 코드를 확인한다.
* `repiu_loader_win32.exe piu_1st`를 실행해 원본 target 경로가 현재 관찰 지점까지 진행되는지 확인한다.

## 검증 기준

* 새 PC에서 `powershell -ExecutionPolicy Bypass -File scripts\setup_test_environment.ps1`가 필수 도구와 자산 상태를 보고한다.
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`가 현재 구현된 두 target을 모두 실행한다.
* 네트워크 또는 외부 설치가 필요한 실패는 코드 실패와 구분되는 메시지로 보고한다.

# Test Environment Bootstrap Design

## Background

Current implementation verification is split across the Win32 x86 host build, the OpenWatcom DOS/4GW sample build, the `dos4gw_hello` run, and the `piu_1st` run.
To perform the same verification on another PC, the environment must quickly check CMake, the Visual Studio 2022 Win32 toolchain, OpenWatcom, and original `PIU.EXE` placement.

Following the existing implementation principle, the original game code remains the primary execution path. This task only automates execution environment preparation and verification.

## Goals

* Keep a script that checks the local prerequisites required for testing.
* Use the CMake bundled with Visual Studio when CMake is not on PATH.
* Install OpenWatcom into `tools/openwatcom/` through the existing `scripts/install_openwatcom.ps1`.
* Keep a verification script that builds the sample DOS/4GW executable and runs all currently implemented loader targets.
* Print clear messages when a required tool or asset is missing.

## Non-Goals

* Do not install Visual Studio, CMake, or Git through a package manager.
* Do not commit OpenWatcom binaries or downloaded files.
* Do not change the loader, HLE, or original executable paths.

## Design

`scripts/setup_test_environment.ps1` does the following:

* Checks Git, CMake, the Visual Studio Win32 toolchain, and `MASTER/PIU_1ST/PIU.EXE`.
* If CMake is not on PATH, locates the Visual Studio bundled CMake with `vswhere.exe` and adds it to the current process PATH.
* If OpenWatcom is missing, calls the existing install script.
* After installing OpenWatcom, runs `scripts/build_dos4gw_hello.bat` to prepare the sample executable.

`scripts/test_all.ps1` does the following:

* Calls `setup_test_environment.ps1` first to prepare missing local test prerequisites.
* Runs `scripts/build_win32_x86.bat` for the Win32 x86 Debug build.
  This batch file calls `scripts/build_win32_x86.ps1`, which selects one of the `Visual Studio 18 2026`, `Visual Studio 17 2022`, or `Visual Studio 16 2019` generators for the installed Visual Studio version.
* Rebuilds the DOS/4GW sample with `scripts/build_dos4gw_hello.bat`.
* Runs `repiu_loader_win32.exe dos4gw_hello` and checks the exit code plus `Hello, world!` output.
* Runs `repiu_loader_win32.exe piu_1st` and confirms the original target path reaches the current observation point.

## Verification Criteria

* On a new PC, `powershell -ExecutionPolicy Bypass -File scripts\setup_test_environment.ps1` reports required tool and asset status.
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1` runs both currently implemented targets.
* Failures requiring network or external installation are reported separately from code failures.
