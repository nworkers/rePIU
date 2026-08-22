# Linux 런처 설계 (Stage 2)

## 배경

[Stage 1](20260822-501-linux-core-build.md)에서 공용 코어와 probe가 i386으로 빌드되고
통과했습니다. Stage 2는 **런처를 Linux에서 띄웁니다.** 실행 엔진은 여전히 Windows
전용이므로 게임은 실행되지 않습니다.

이 단계의 진짜 목적은 화면이 아니라 **32비트 SDL3 데스크톱 스택이 서는지 확인하는
것**입니다. 실행 엔진도 SDL을 씁니다 — 게임 창(`glide_opengl_backend.cpp`)과 오디오
세 경로가 모두 SDL 기반입니다. 따라서 i386 SDL이 안 서면 Stage 3도 막힙니다. 엔진에
몇 주를 쓴 뒤가 아니라 지금 알아야 합니다.

## 결정 1: 런처는 Linux 전용 진입점을 갖습니다

Windows에서 `repiu.exe`는 런처와 로더를 겸합니다. 인자가 없으면 런처, 있으면 실행
엔진입니다. Linux에는 아직 엔진이 없으므로 그 구조를 그대로 옮길 수 없습니다.

`src/host/linux/main.cpp`를 새로 두고 **런처 화면(`launcher_ui.cpp`)만 공유**합니다.
Windows 쪽의 복귀 루프와 자식 프로세스 생성은 가져오지 않습니다 — 둘 다 Win32 고유
사정에서 나온 것이고(GPU 드라이버가 게스트 주소 공간을 선점하는 순서 문제), Linux에서
같은 제약이 있을지는 Stage 3에서 실제 엔진을 붙여봐야 압니다.

선택이 이루어지면 **"실행 엔진이 아직 없다"고 명시하고 종료합니다.** 없는 것을 있는 척
실행해 "파일 없음"으로 실패하게 두는 것보다 낫습니다.

## 결정 2: 쓰지 않는 X11 확장은 설치 대신 끕니다

SDL3의 i386 configure가 `XSCRNSAVER`와 `XTEST`를 요구하며 멈췄습니다. 각각 화면보호기
억제와 입력 시뮬레이션용이고 이 프로젝트는 둘 다 쓰지 않습니다.

**패키지를 더 요구하는 대신 껐습니다.** 켜 둔 확장 하나가 곧 운영자가 찾아야 할 32비트
개발 패키지 하나이고, i386 패키지는 amd64 쪽과 충돌하기 쉬워 설치 부담이 실제 장벽이
됩니다. 결과 구성은 X11 비디오, OpenGL 렌더, ALSA 오디오로 런처와 향후 게임 창에 필요한
것이 모두 들어 있습니다.

## 결정 3: OpenGL은 이름으로 링크합니다

런처는 자기 뷰포트를 직접 지우고 크기를 맞추므로 `glViewport`·`glClear`가 필요합니다.
Windows에서는 `opengl32`가 코어 라이브러리를 통해 따라오지만 Linux에서는 명시해야
합니다.

`find_package(OpenGL)`의 `OpenGL::GL` 대신 **평범한 `GL`** 을 씁니다. 전자는 기본 경로를
먼저 뒤져 64비트 `libGL.so`를 잡을 수 있고, `-m32` 빌드에서는 아키텍처 불일치로 링크가
깨집니다. 이름만 주면 `-m32`가 이미 검색 경로에 올린 `/usr/lib/i386-linux-gnu`에서
해결됩니다. **멀티립 빌드에서 시스템 라이브러리를 링크할 때 계속 적용할 규칙입니다.**

## 결정 4: 빌드 스크립트에 `--headless`를 둡니다

Stage 1은 데스크톱 패키지 없이 코어만 빌드할 수 있었고 그 경로를 README가 안내합니다.
Stage 2는 데스크톱이 필요하므로 기본값을 데스크톱 빌드로 바꾸고, 코어만 빌드하려는
경우를 위해 `--headless`를 남깁니다.

## 범위 밖

* 실행 엔진 (Stage 3)
* Linux에서 게임 실행, CHD 마운트 검증
* 발판 입력, 인게임 OSD
* Wayland 백엔드 — WSLg가 X 서버를 함께 제공하므로 X11로 충분합니다

## 검증

* i386 `repiu_launcher`가 빌드되어야 합니다.
* WSLg에서 실행해 창이 유지되고, 롬셋 목록이 실제 `roms/` 내용을 반영해야 합니다.
* 종료 신호에 깨끗하게 닫혀야 합니다.
* Windows 빌드에 회귀가 없어야 합니다.

---

# Linux Launcher Design (Stage 2)

## Background

Stage 1 got the neutral core and its probes building and passing as i386. Stage 2 brings the
launcher up on Linux; the execution engine is still Windows-only, so no game runs. The real point
of this stage is not the screen but finding out whether the 32-bit SDL3 desktop stack stands up at
all, because the engine uses SDL too — the game window and all three audio paths — so a failure
here would block Stage 3. Better to learn it now than after weeks of engine work.

## Decisions

**A Linux-only entry point.** On Windows `repiu.exe` is both launcher and loader, chosen by
whether an argument is present. Linux has no engine yet, so `src/host/linux/main.cpp` shares only
the launcher screen. The Windows return loop and child process are not carried over: both exist
for Win32-specific reasons — a GPU driver claiming the guest's low address space — and whether the
same constraint applies on Linux can only be answered once a real engine exists there. A selection
reports that the engine is unavailable rather than spawning something that cannot exist.

**Unused X11 extensions are switched off rather than installed.** SDL3's i386 configure stopped on
`XSCRNSAVER` and `XTEST`, which inhibit the screen saver and simulate input; this project uses
neither. Every extension left enabled is one more 32-bit development package an operator has to
find, and i386 packages conflict with their amd64 counterparts easily enough that the install is a
real barrier. What remains — X11 video, OpenGL rendering, ALSA audio — covers the launcher and the
future game window.

**OpenGL is linked by name.** The launcher clears and sizes its own viewport, and on Windows those
symbols arrive with `opengl32` through the core library. On Linux the library must be named, and
plain `GL` is used rather than `OpenGL::GL`, because the imported target searches default paths
first and can bind the 64-bit `libGL.so`, which fails an `-m32` link on architecture. The bare name
resolves in the i386 directory that `-m32` already puts on the search path. This rule applies to
every system library the multilib build links from here on.

**The build script gains `--headless`.** Stage 1 could build the core without desktop packages and
the README documents that path; Stage 2 needs the desktop, so the default flips and the flag keeps
the smaller dependency set available.

## Out of scope

The execution engine, running a game or verifying the CHD mount on Linux, panel input, the in-game
OSD, and the Wayland backend, since WSLg also provides an X server.

## Verification

An i386 `repiu_launcher` builds; it runs under WSLg with the window staying alive and the ROM set
list reflecting the real `roms/` contents; it closes cleanly on a termination signal; and the
Windows build shows no regression.
