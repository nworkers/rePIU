# Linux 런처 작업 지시 (Stage 2)

설계: [20260822-502-linux-launcher.md](../design/20260822-502-linux-launcher.md)

1. 32비트 SDL3 데스크톱 configure가 서는지 먼저 확인합니다. 막히는 X11 확장은 이
   프로젝트가 쓰지 않는 것에 한해 끄고, 그 판단을 빌드 스크립트 주석에 남깁니다.
2. `src/host/linux/main.cpp`를 추가합니다. 설정을 읽고, 카탈로그를 만들고, 런처 화면을
   띄우고, 변경된 설정을 저장합니다. 선택이 이루어지면 실행 엔진이 아직 없다고 알리고
   종료합니다.
3. `if(UNIX)` 아래 `repiu_launcher` 타깃을 추가하고 `launcher_ui.cpp`를 Windows와
   공유합니다. OpenGL은 멀티립에서 올바로 해결되도록 이름으로 링크합니다.
4. `scripts/build_linux_i386.sh`의 기본값을 데스크톱 빌드로 바꾸고 `--headless`를
   남깁니다.
5. WSLg에서 i386 런처를 실행해 창이 유지되고 목록이 실제 `roms/`를 반영하는지 확인합니다.
6. Windows Debug에서 `repiu`와 `repiu_core_probe`를 다시 빌드해 회귀가 없음을 확인합니다.
7. `README.md`와 `ARCHITECTURE.md`에 Linux 런처 빌드와 현재 범위를 반영합니다.

## 완료 조건

i386 `repiu_launcher`가 빌드되고 WSLg에서 실행되며, 롬셋 목록이 실제 자산을 반영하고,
종료 신호에 깨끗하게 닫혀야 합니다. Windows 빌드에 회귀가 없어야 합니다. 게임 실행은 이
작업의 완료 조건이 아닙니다.

---

# Linux Launcher Work Order (Stage 2)

Design: [20260822-502-linux-launcher.md](../design/20260822-502-linux-launcher.md)

1. Confirm first that a 32-bit SDL3 desktop configure stands up, switching off only the X11
   extensions this project does not use and recording that judgement in the build script.
2. Add `src/host/linux/main.cpp`: load settings, build the catalog, run the launcher, save changed
   settings, and report that no execution engine exists yet when a ROM set is chosen.
3. Add a `repiu_launcher` target under `if(UNIX)` sharing `launcher_ui.cpp` with Windows, linking
   OpenGL by name so multilib resolves it correctly.
4. Make the desktop build the default in `scripts/build_linux_i386.sh` and keep `--headless`.
5. Run the i386 launcher under WSLg and confirm the window stays alive with a list reflecting the
   real `roms/`.
6. Rebuild `repiu` and `repiu_core_probe` for Windows Debug and confirm no regression.
7. Document the Linux launcher build and its current scope in `README.md` and `ARCHITECTURE.md`.

## Completion criteria

An i386 `repiu_launcher` builds, runs under WSLg with a list reflecting the real assets, and closes
cleanly on a termination signal, with no regression in the Windows build. Running a game is not
part of this task.
