# 테스트 환경 부트스트랩 작업 로그

## 결과

다른 PC에서 현재 구현 내용을 한 번에 검증할 수 있도록 테스트 환경 준비와 전체 검증 스크립트를 추가했다.

구현 내용은 다음과 같다.

* `scripts/setup_test_environment.ps1`를 추가했다.
  * Git, CMake, Visual Studio C++ Win32 도구체인, 원본 `MASTER/PIU_1ST/PIU.EXE`, OpenWatcom 상태를 확인한다.
  * CMake가 PATH에 없으면 Visual Studio 번들 CMake를 찾아 현재 프로세스 PATH에 추가한다.
  * OpenWatcom이 없으면 기존 `scripts/install_openwatcom.ps1`를 호출해 `tools/openwatcom/`에 설치한다.
  * DOS/4GW hello 샘플을 빌드해 `samples/dos4gw_hello/build/hello.exe`를 준비한다.
* `scripts/build_win32_x86.ps1`를 추가하고 `scripts/build_win32_x86.bat`가 이를 호출하게 변경했다.
  * 설치된 Visual Studio 버전에 맞춰 `Visual Studio 18 2026`, `Visual Studio 17 2022`, `Visual Studio 16 2019` generator 중 하나를 선택한다.
  * Win32 Debug 빌드 출력 위치를 `build/win32_x86_debug/`로 둔다.
* `scripts/test_all.ps1`를 추가했다.
  * 테스트 환경 준비, Win32 x86 빌드, DOS/4GW 샘플 빌드, `dos4gw_hello` 실행, `piu_1st` 실행을 순서대로 수행한다.
  * loader stderr 로그를 PowerShell 오류로 취급하지 않고 검증 출력으로 캡처한다.
* 설계 문서와 작업 지시 문서를 추가했다.

## 현재 PC에서 확인한 누락 환경

초기 상태에서 다음 항목이 누락되어 있었다.

* `cmake`가 PATH에 없었다.
  * Visual Studio Community 2026 번들 CMake를 감지해 사용했다.
* `tools/openwatcom/`이 없었다.
  * 네트워크 권한을 받아 OpenWatcom을 다운로드하고 설치했다.
* 기존 `scripts/build_win32_x86.bat`는 `Visual Studio 17 2022` generator에 고정되어 있었다.
  * 현재 PC의 Visual Studio 2026 환경에서는 v143 toolset이 없어 실패했으므로 generator 자동 선택으로 보완했다.
* `spdlog` FetchContent 다운로드는 네트워크 권한 없이 실패했다.
  * 네트워크 권한을 받아 `spdlog`를 다운로드한 뒤 빌드가 성공했다.

## 검증

`powershell -ExecutionPolicy Bypass -File scripts\setup_test_environment.ps1`

결과: 성공.

* Git 확인 성공.
* Visual Studio 번들 CMake 확인 성공.
* Visual Studio C++ 도구체인 확인 성공.
* 원본 `MASTER\PIU_1ST\PIU.EXE` 확인 성공.
* OpenWatcom 다운로드 및 설치 성공.
* DOS/4GW hello 샘플 빌드 성공.

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

결과: 성공.

* Win32 x86 Debug 빌드 성공.
* `dos4gw_hello` target 실행 성공.
  * `Hello, world!` 출력 확인.
  * original entry가 host trampoline으로 반환됨.
* `piu_1st` target 실행 성공.
  * handled HLE trap count: 1
  * handled DOS interrupt count: 2
  * handled segment load count: 3
  * handled segment store count: 1
  * 현재 관찰 지점: `26 8A 4F FF`

기존과 동일하게 외부 `spdlog` header에서 MSVC C4819 경고가 발생할 수 있으나 빌드는 성공한다.

## 다음 작업

다음 실제 구현 작업은 기존 TODO와 동일하게 `26 8A 4F FF` segment-override byte memory load에 대한 selector shadow state 및 주소 변환 정책을 명확히 하고 구현하는 것이다.

# Test Environment Bootstrap Work Log

## Result

Added test environment setup and full verification scripts so another PC can validate all currently implemented behavior in one flow.

Implemented changes:

* Added `scripts/setup_test_environment.ps1`.
  * Checks Git, CMake, the Visual Studio C++ Win32 toolchain, original `MASTER/PIU_1ST/PIU.EXE`, and OpenWatcom status.
  * If CMake is not on PATH, locates the Visual Studio bundled CMake and adds it to the current process PATH.
  * If OpenWatcom is missing, calls the existing `scripts/install_openwatcom.ps1` and installs it into `tools/openwatcom/`.
  * Builds the DOS/4GW hello sample as `samples/dos4gw_hello/build/hello.exe`.
* Added `scripts/build_win32_x86.ps1` and changed `scripts/build_win32_x86.bat` to call it.
  * Selects one of the `Visual Studio 18 2026`, `Visual Studio 17 2022`, or `Visual Studio 16 2019` generators for the installed Visual Studio version.
  * Uses `build/win32_x86_debug/` as the Win32 Debug build output directory.
* Added `scripts/test_all.ps1`.
  * Runs environment setup, Win32 x86 build, DOS/4GW sample build, `dos4gw_hello`, and `piu_1st` in order.
  * Captures loader stderr logs as verification output instead of treating them as PowerShell errors.
* Added the design document and work order.

## Missing Environment Found On This PC

The initial state was missing the following pieces:

* `cmake` was not on PATH.
  * The script detected and used the Visual Studio Community 2026 bundled CMake.
* `tools/openwatcom/` was missing.
  * OpenWatcom was downloaded and installed after network permission was granted.
* The existing `scripts/build_win32_x86.bat` was fixed to the `Visual Studio 17 2022` generator.
  * This failed on the current Visual Studio 2026 PC because the v143 toolset was not installed, so generator auto-selection was added.
* `spdlog` FetchContent download failed without network permission.
  * After network permission was granted, `spdlog` downloaded and the build succeeded.

## Verification

`powershell -ExecutionPolicy Bypass -File scripts\setup_test_environment.ps1`

Result: success.

* Git check succeeded.
* Visual Studio bundled CMake check succeeded.
* Visual Studio C++ toolchain check succeeded.
* Original `MASTER\PIU_1ST\PIU.EXE` check succeeded.
* OpenWatcom download and install succeeded.
* DOS/4GW hello sample build succeeded.

`powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

Result: success.

* Win32 x86 Debug build succeeded.
* `dos4gw_hello` target run succeeded.
  * Confirmed `Hello, world!` output.
  * The original entry returned to the host trampoline.
* `piu_1st` target run succeeded.
  * handled HLE trap count: 1
  * handled DOS interrupt count: 2
  * handled segment load count: 3
  * handled segment store count: 1
  * current observation point: `26 8A 4F FF`

As before, the external `spdlog` header may emit MSVC C4819 warnings, but the build succeeds.

## Next Work

The next real implementation task remains clarifying and implementing selector shadow state plus address translation policy for the `26 8A 4F FF` segment-override byte memory load.
