# 테스트 환경 부트스트랩 작업 지시

## 작업 항목

1. 현재 검증에 필요한 도구와 자산 전제 조건을 정리한다.
2. `scripts/setup_test_environment.ps1`를 추가해 Git, CMake, Visual Studio Win32 도구체인, OpenWatcom, 원본 `PIU.EXE` 상태를 점검한다.
3. CMake가 PATH에 없을 때 Visual Studio 번들 CMake를 찾아 현재 세션에서 사용할 수 있게 한다.
4. OpenWatcom이 없으면 기존 `scripts/install_openwatcom.ps1`를 호출해 로컬 도구를 준비한다.
5. `scripts/test_all.ps1`를 추가해 Win32 x86 빌드, DOS/4GW 샘플 빌드, `dos4gw_hello` 실행, `piu_1st` 실행을 한 번에 수행한다.
6. `scripts/build_win32_x86.bat`가 설치된 Visual Studio 버전에 맞는 Win32 generator를 자동 선택하게 한다.
7. 가능한 범위에서 새 스크립트를 실행해 현재 PC의 누락 환경을 확인한다.
8. 작업 로그를 남긴다.

## 비목표

* Visual Studio, CMake, Git의 자동 설치
* OpenWatcom 바이너리 커밋
* loader/HLE 코드 변경

# Test Environment Bootstrap Work Order

## Tasks

1. Summarize the tool and asset prerequisites required by current verification.
2. Add `scripts/setup_test_environment.ps1` to check Git, CMake, the Visual Studio Win32 toolchain, OpenWatcom, and original `PIU.EXE` status.
3. Locate Visual Studio bundled CMake for the current session when CMake is not on PATH.
4. If OpenWatcom is missing, call the existing `scripts/install_openwatcom.ps1` to prepare the local tool.
5. Add `scripts/test_all.ps1` to run the Win32 x86 build, DOS/4GW sample build, `dos4gw_hello`, and `piu_1st` in one command.
6. Make `scripts/build_win32_x86.bat` automatically select the Win32 generator that matches the installed Visual Studio version.
7. Run the new scripts where possible to identify missing environment pieces on the current PC.
8. Leave a work log.

## Non-Goals

* Automatic installation of Visual Studio, CMake, or Git.
* Committing OpenWatcom binaries.
* Changing loader/HLE code.
