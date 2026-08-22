# Linux 런처 작업 로그 (Stage 2)

설계: [20260822-502-linux-launcher.md](../design/20260822-502-linux-launcher.md)

작업 지시: [20260822-502-linux-launcher.md](../work-orders/20260822-502-linux-launcher.md)

## 1. 결과

**i386 런처가 WSLg에서 돕니다.** 롬셋 목록이 실제 `roms/`를 반영하고, 종료 신호에 깨끗하게
닫힙니다. 실행 엔진은 여전히 Windows 전용이라 게임은 실행되지 않습니다.

| 파일 | 내용 |
|---|---|
| `src/host/linux/main.cpp` | Linux 런처 진입점 |
| `CMakeLists.txt` | `if(UNIX)` 아래 `repiu_launcher`, OpenGL 링크 |
| `scripts/build_linux_i386.sh` | 기본을 데스크톱 빌드로, `--headless` 유지, 미사용 X11 확장 비활성 |

## 2. 이 단계의 진짜 목적은 화면이 아니었습니다

사용자 질문("런처와 실행엔진이 다른가?")이 계획의 결함을 드러냈습니다. 저는 직전에
"Stage 2를 x86_64로 먼저 하자"고 권했는데, 두 가지가 어긋났습니다.

* `repiu.exe`는 **런처와 로더를 겸하는 하나의 바이너리**입니다. 런처만 64비트로 만들면 그
  바이너리는 실행 엔진을 담을 수 없습니다.
* 더 중요하게, **실행 엔진도 SDL을 씁니다** — 게임 창과 오디오 세 경로가 모두 SDL
  기반입니다. 즉 i386 SDL 스택은 Stage 3에서 어차피 필요하고, 64비트로 우회하면 가장
  불확실한 환경 문제를 엔진 작업 뒤로 미루는 셈이었습니다.

그래서 i386으로 바로 갔고, **그 판단이 이 단계에서 가장 중요한 결과를 냈습니다.**

## 3. 32비트 SDL3 데스크톱 스택은 섭니다 (확인됨)

```
Video drivers: dummy offscreen x11(dynamic)
X11 libraries: xcursor xdbe xfixes xinput2 xrandr xshape xsync
Render drivers: gpu ogl ogl_es2 vulkan
Audio drivers: alsa(dynamic) disk dummy
```

X11 비디오 + OpenGL 렌더 + ALSA 오디오. 런처뿐 아니라 **Stage 3의 게임 창과 사운드까지
쓸 수 있는 구성**입니다. 포팅 전체를 막을 수 있었던 위험이 여기서 해소됐습니다.

Wayland는 잡히지 않았지만 WSLg가 X 서버를 함께 제공하므로 문제되지 않습니다.

## 4. 판단들

### 4.1 쓰지 않는 X11 확장은 설치 대신 껐습니다

configure가 `XSCRNSAVER`와 `XTEST`에서 멈췄습니다. 화면보호기 억제와 입력 시뮬레이션용
확장이고 이 프로젝트는 둘 다 쓰지 않습니다. 패키지를 더 요구하는 대신 껐습니다 — **켜 둔
확장 하나가 곧 운영자가 찾아야 할 32비트 패키지 하나**이고, i386 패키지는 amd64와 충돌하기
쉬워 설치가 실제 장벽이 됩니다.

### 4.2 OpenGL은 이름으로 링크합니다

첫 빌드가 `glViewport`·`glClear` 미해결로 실패했습니다. Windows에서는 `opengl32`가 코어를
통해 따라오지만 Linux에서는 명시해야 합니다.

`find_package(OpenGL)`의 `OpenGL::GL` 대신 **평범한 `GL`** 을 씁니다. 전자는 기본 경로를
먼저 뒤져 64비트 `libGL.so`를 잡을 수 있고 `-m32` 빌드에서 아키텍처 불일치로 깨집니다.
이름만 주면 `-m32`가 이미 올려둔 `/usr/lib/i386-linux-gnu`에서 해결됩니다. **멀티립에서
시스템 라이브러리를 링크할 때 계속 쓸 규칙**이라 주석으로 남겼습니다.

### 4.3 Linux 진입점은 Windows 구조를 그대로 옮기지 않았습니다

Windows의 복귀 루프와 자식 프로세스 생성은 **Win32 고유 사정**(GPU 드라이버가 게스트
저주소 공간을 선점하는 순서)에서 나온 것입니다. Linux에서 같은 제약이 있을지는 실제
엔진을 붙여봐야 알 수 있으므로 가져오지 않았습니다. 공유하는 것은 런처 화면뿐입니다.

선택이 이루어지면 **"실행 엔진이 아직 없다"고 명시하고 종료**합니다. 없는 것을 spawn해서
"파일 없음"으로 실패하게 두는 것보다 낫습니다.

## 5. 검증

* i386 `repiu_launcher` 빌드 성공: `ELF 32-bit LSB pie executable, Intel 80386`.
* WSLg 실행: `rom sets: 22 listed, 16 runnable` — 카탈로그가 실제 `roms/`를 읽었습니다.
* **창이 7.14초 동안 살아 있었고**, 8초에 보낸 종료 신호를 SDL이 quit 이벤트로 바꿔 깨끗하게
  닫혔습니다. `timeout`의 `exit=124`는 시그널로 끝났다는 뜻이지 오류가 아닙니다.
  이 구분을 위해 UI 실행 시간을 출력하도록 계측을 남겼습니다 — **즉시 닫힌 런처와 아예
  뜨지 않은 런처는 프로세스 밖에서 보면 똑같습니다.**
* Windows Debug `repiu` 재빌드 오류 0, `repiu_core_probe` 9항목 통과.

**아직 확인하지 못한 것:** 창이 실제로 눈에 보이고 조작되는지는 사람이 봐야 합니다.

## 6. 남은 것

* Stage 3: 실행 엔진. VEH → POSIX 시그널, `CONTEXT` 270곳 → 자체 레지스터 구조체,
  `VirtualProtect` 47곳 → `mprotect`, `fs:[...]` 28곳 → Linux 세그먼트. SMC 감지의 Linux
  대응이 핵심 난제입니다.
* Linux 런처에서 게임을 실행하는 배선은 Stage 3에서 채워집니다. 그때 Windows가 겪은
  주소 공간 선점 문제가 Linux에도 있는지 확인해야 합니다.
* 런처 진입점의 중복(설정 읽기·저장 흐름)은 지금은 작지만, Linux가 엔진을 갖게 되면
  공용 세션으로 뽑는 것이 맞습니다.

---

# Linux Launcher Work Log (Stage 2)

Design: [20260822-502-linux-launcher.md](../design/20260822-502-linux-launcher.md)

Work order: [20260822-502-linux-launcher.md](../work-orders/20260822-502-linux-launcher.md)

## Result

The i386 launcher runs under WSLg, lists the real contents of `roms/`, and closes cleanly on a
termination signal. The execution engine is still Windows-only, so no game runs.

## The point of this stage was not the screen

A user question — are the launcher and the execution engine different things? — exposed a flaw in
the plan. The previous recommendation had been to do Stage 2 as x86_64, and two things were wrong
with it: `repiu.exe` is a single binary that is both launcher and loader, so a 64-bit launcher
could not host the engine; and more importantly the engine uses SDL too, for the game window and
all three audio paths. The i386 SDL stack was therefore unavoidable for Stage 3, and taking the
64-bit shortcut would have deferred the least certain part of the whole port until after the
engine work. Going straight to i386 produced this stage's most valuable result.

## The 32-bit SDL3 desktop stack stands up

X11 video, OpenGL rendering, and ALSA audio all configured for i386 — enough not only for the
launcher but for the game window and sound in Stage 3. The risk that could have blocked the entire
port is retired. Wayland was not detected, which does not matter because WSLg also provides an X
server.

## Judgement calls

**Unused X11 extensions were switched off rather than installed.** Configure stopped on
`XSCRNSAVER` and `XTEST`, which inhibit the screen saver and simulate input; neither is used here.
Every extension left on is one more 32-bit package an operator has to find, and i386 packages
conflict with their amd64 counterparts easily enough that the install is the real barrier.

**OpenGL is linked by name.** The first build failed on unresolved `glViewport` and `glClear`,
which arrive with `opengl32` through the core library on Windows. Plain `GL` is used instead of
`OpenGL::GL`, because the imported target searches default paths first and can bind the 64-bit
`libGL.so`, failing an `-m32` link on architecture; the bare name resolves in the i386 directory
`-m32` already searches. That rule now carries a comment, since every further system library in
the multilib build faces it.

**The Linux entry point does not copy the Windows structure.** The return loop and the child
process exist for Win32-specific reasons, and whether the same constraint applies on Linux can
only be answered with a real engine there. A selection reports that the engine is unavailable
rather than spawning something that cannot exist.

## Verification

The i386 binary is an `ELF 32-bit LSB pie executable, Intel 80386`. Under WSLg it reported `rom
sets: 22 listed, 16 runnable`, kept its window alive for 7.14 seconds, and shut down cleanly when
signalled — `timeout`'s exit code 124 means it was signalled, not that it failed. The UI lifetime
is printed deliberately, because a launcher that closes instantly and one that never appears look
identical from outside the process. Windows Debug rebuilt `repiu` with zero errors and
`repiu_core_probe` still passes all nine checks. What remains unverified is whether the window is
actually visible and operable, which needs a person.

## Remaining

Stage 3 ports the execution engine — vectored handlers to POSIX signals, 270 `CONTEXT` uses to a
rePIU-owned register structure, 47 `VirtualProtect` calls to `mprotect`, 28 `fs:[...]` sites to
Linux segments — with self-modifying-code detection as the central problem. Wiring the Linux
launcher to actually start a game belongs to that stage, including finding out whether the address
space ordering that forced a separate process on Windows applies there too. The small duplication
between the two entry points should become a shared session once Linux has an engine.
