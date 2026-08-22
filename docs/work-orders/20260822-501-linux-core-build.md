# Linux 코어 빌드 작업 지시 (Stage 1)

설계: [20260822-501-linux-core-build.md](../design/20260822-501-linux-core-build.md)

1. `CMakeLists.txt`에서 `repiu_exe`의 `src/platform/win32` 소스를 `if(WIN32)`로 감싸고,
   Windows 전용 실행 파일 타깃(`repiu`, `repiu_aot_probe`, supervisor, analyzer, glide
   probe 등)도 같은 가드 안으로 옮깁니다. Windows 빌드 결과는 바뀌지 않아야 합니다.
2. Win32에 의존하지 않는 probe 14개를 담는 `repiu_core_probe` 타깃을 추가하고, 두
   플랫폼에서 모두 빌드되게 합니다. probe 진입점은 인자 없이 전부 실행하고 실패 시
   비영 종료 코드를 돌려줍니다.
3. `scripts/build_linux_i386.sh`를 추가합니다. configuration과 target을 받고,
   `build/linux_i386`에 빌드하며, 32비트 툴체인 패키지가 없으면 설치 명령을 안내하고
   종료합니다.
4. SDL3·spdlog·minimp3·libchdr가 Linux i386에서 어떻게 다뤄지는지 확인하고, 코어가
   실제로 필요로 하는 것만 남깁니다. 필요 없으면 Linux 구성에서 제외합니다.
5. WSL Ubuntu 24.04에서 i386으로 빌드하고 `repiu_core_probe`를 실행합니다.
6. Windows Debug/Release에서 `repiu`와 `repiu_aot_probe`를 다시 빌드하고 전체 probe를
   실행해 회귀가 없음을 확인합니다.
7. `ARCHITECTURE.md`와 `README.md`에 Linux 빌드 방법과 현재 지원 범위를 적습니다.

## 완료 조건

Linux에서 `repiu_exe`와 `repiu_core_probe`가 i386으로 빌드되고 probe가 통과해야 합니다.
같은 probe가 Windows에서도 같은 결과를 내야 하며, 기존 Windows 빌드와 probe에 회귀가
없어야 합니다. 게임 실행은 이 작업의 완료 조건이 아닙니다.

---

# Linux Core Build Work Order (Stage 1)

Design: [20260822-501-linux-core-build.md](../design/20260822-501-linux-core-build.md)

1. Wrap `repiu_exe`'s `src/platform/win32` sources in `if(WIN32)` and move the Windows-only
   executable targets into the same guard, leaving the Windows build's output unchanged.
2. Add a `repiu_core_probe` target holding the 14 Win32-free probes, building on both platforms,
   whose entry point runs them all and returns non-zero on failure.
3. Add `scripts/build_linux_i386.sh` taking a configuration and targets, building into
   `build/linux_i386`, and reporting the install command when the 32-bit toolchain packages are
   missing.
4. Check how SDL3, spdlog, minimp3, and libchdr behave for Linux i386 and keep only what the core
   actually needs, excluding the rest from the Linux configuration.
5. Build i386 under WSL Ubuntu 24.04 and run `repiu_core_probe`.
6. Rebuild `repiu` and `repiu_aot_probe` for Windows Debug and Release and run the full probe to
   confirm no regression.
7. Document the Linux build and its current scope in `ARCHITECTURE.md` and `README.md`.

## Completion criteria

`repiu_exe` and `repiu_core_probe` build as i386 on Linux and the probe passes there, the same
probe produces the same results on Windows, and the existing Windows builds and probes show no
regression. Running a game is not part of this task.
