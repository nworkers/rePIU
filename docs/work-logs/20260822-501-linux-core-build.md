# Linux 코어 빌드 작업 로그 (Stage 1)

설계: [20260822-501-linux-core-build.md](../design/20260822-501-linux-core-build.md)

작업 지시: [20260822-501-linux-core-build.md](../work-orders/20260822-501-linux-core-build.md)

## 1. 결과

플랫폼 공용 코어와 그 probe가 Linux에서 빌드되고 통과합니다. 실행 엔진·로더·런처는 아직
Windows 전용이며, 이번 작업은 **게임을 실행하지 않습니다.**

| 파일 | 내용 |
|---|---|
| `CMakeLists.txt` | `src/platform/win32` 76개와 Windows 전용 실행 파일을 `if(WIN32)`로 격리, `repiu_core_probe` 신설 |
| `scripts/build_linux_i386.sh` | i386 빌드, 툴체인 사전 확인, `build/linux_i386` |
| `src/tools/core_probe/main.cpp` | 플랫폼 무관 probe 9개를 한 실행 파일로 |
| `src/tools/aot_probe/nvram_path_probe.cpp` | MSVC 전용 `_putenv`를 POSIX와 분기 |

## 2. 측정이 설계를 결정했습니다

이식 범위를 추정하지 않고 먼저 쟀습니다.

| 측정 | 결과 |
|---|---:|
| 전체 소스 / `src/platform/win32` / 공용 코어 | 192 / 76 / 53 |
| 공용 코어가 win32 헤더를 include | **0** |
| 공용 코어의 MSVC 전용 구문 | **0** |
| 공용 코어의 32비트 포인터 가정 | **0** |
| 공용 코어 `g++ -std=c++20 -fsyntax-only` | **오류 0** |

**코어는 한 줄도 고치지 않고 GCC로 넘어갔습니다.** `AGENTS.md`의 "플랫폼 공용 구조를 우선
설계한다"가 실제로 지켜져 있던 덕이고, 이 측정이 없었다면 훨씬 큰 작업으로 예상했을
것입니다.

반대쪽은 큽니다 — `CONTEXT` 270곳, `VirtualProtect` 47곳, `fs:[...]` 어셈블리 28곳,
`VirtualQuery` 15곳, `VirtualAlloc` 8곳, VEH 3곳. Stage 3의 규모입니다.

## 3. 판단들

### 3.1 새 라이브러리 타깃 대신 조건부 소스

`repiu_exe` 하나에 코어와 Win32 계층이 함께 있었습니다. 타깃을 쪼개면 링크와 include
배선을 전부 다시 해야 하므로, 같은 타깃 안에서 Win32 소스만 `if(WIN32)`로 감쌌습니다.
Windows 결과물과 링크 구성은 그대로입니다.

**옮기다 발견한 것:** win32 소스 블록 사이에 `src/sound/*` 7개가 끼어 있었습니다. 줄
범위로 통째로 옮겼다면 사운드 코어가 조용히 Windows 전용이 될 뻔했습니다. 파일 단위로
걸러 제자리에 뒀습니다.

### 3.2 probe 구성원은 grep이 아니라 컴파일로 정해야 했습니다

처음에 `windows.h`와 `platform/win32` 문자열로 grep해 "Win32-free probe 14개"라고
판단했고, **틀렸습니다.** probe들은 include 디렉터리를 통해
`dos/dos_int21_services.h`, `cpu_emul/instruction_emulation.h`처럼 **상대 경로로** win32
헤더를 끌어옵니다. 문자열 검색으로는 보이지 않습니다.

실제로 컴파일하고 링크해 다시 가린 결과는 **9개**입니다: `env_toggle`,
`execution_backend`, `execution_timeout`, `dos_file_handle_cache`, `pit_timer`,
`glide_lfb_region`, `jump_table_guard`, `nvram_path`, `launcher`.

중간에 링크 실패 2건(`launcher`, `nvram_path`)이 나왔지만 원인은 플랫폼이 아니라 **제
수동 링크 명령이 libchdr의 LZMA 의존을 빠뜨린 것**이었습니다. CMake에서는 자동으로
붙습니다 — 도구를 손으로 흉내 내면 도구가 이미 하던 일을 잊는다는 사례입니다.

판정 방법을 `core_probe/main.cpp` 주석에 남겼습니다.

### 3.3 SDL3는 헤드리스 옵션으로 우회했습니다

SDL3는 X11 또는 Wayland 개발 패키지가 없으면 configure를 **거부합니다.** Stage 1은 창을
열지 않으므로 SDL이 문서화한 `-DSDL_UNIX_CONSOLE_BUILD=ON`을 씁니다. 코어가 SDL을 쓰는
것은 `SDL_keycode.h`의 키 상수뿐이라 기능 손실이 없습니다. **Stage 2(런처)에서는 진짜
데스크톱 패키지를 설치해야 합니다.**

### 3.4 32비트 툴체인은 먼저 확인하고 안내합니다

빌드 스크립트가 `cc -m32`/`c++ -m32`를 먼저 시험하고, 실패하면 설치 명령을 출력하고
종료합니다. 없으면 헤더 오류 수백 줄에 원인이 묻힙니다.

## 4. 발견한 이식성 결함

**`nvram_path_probe.cpp`의 `_putenv`.** MSVC 전용입니다. POSIX는 `setenv`/`unsetenv`이고,
변수를 지우는 방식도 다릅니다(Windows는 빈 값 대입). 조건부로 나눴습니다.

**공용 코어 53개에서는 이런 결함이 하나도 나오지 않았습니다.** 결함은 probe에서만
나왔는데, probe는 애초에 플랫폼 공용으로 설계된 적이 없으니 자연스러운 결과입니다.

## 5. 검증

* Linux(WSL2 Ubuntu 24.04, GCC 13.3)에서 `librepiu_exe.a` 빌드 성공.
* Linux에서 `repiu_core_probe` 9항목 전부 통과(`core_probe_failures=0`).
* Windows Debug에서 `repiu_aot_probe` 오류 0 — CMake 구조 변경에 회귀 없음.
* Windows에서도 `repiu_core_probe` 9항목 통과. **두 실행의 출력 46줄이 완전히
  동일합니다** — 이 타깃을 따로 만든 이유가 "통과했다"가 아니라 "같은 코드가 두 OS에서
  같은 결과를 낸다"를 확인하는 것이었고, 그것이 확인됐습니다.
* i386 산출물 확인: `ELF 32-bit LSB pie executable, Intel 80386`.

## 6. 남은 것

* Stage 2: 런처를 WSLg에서. SDL3와 ImGui의 i386 빌드가 필요하고, 32비트 데스크톱 개발
  패키지 설치가 전제입니다.
* Stage 3: 실행 엔진. VEH → POSIX 시그널, `CONTEXT` → rePIU 소유 레지스터 구조체,
  `VirtualProtect` → `mprotect`, `fs:[...]` → Linux 세그먼트/TLS. SMC 감지의 Linux 대응이
  핵심 난제입니다.
* `repiu_chd_cd_probe`와 `repiu_glide_issue_probe`는 win32 참조가 없어 가드하지 않았지만,
  Linux에서 실제로 빌드되는지는 아직 확인하지 않았습니다.

---

# Linux Core Build Work Log (Stage 1)

Design: [20260822-501-linux-core-build.md](../design/20260822-501-linux-core-build.md)

Work order: [20260822-501-linux-core-build.md](../work-orders/20260822-501-linux-core-build.md)

## Result

The platform-neutral core and its probes build and pass on Linux. The execution engine, the
loader, and the launcher remain Windows-only, and this stage deliberately runs no game.

## Measurement decided the design

Of 192 sources, 76 are the Win32 layer and 53 are the neutral core, and that core includes no
Win32 header, uses no MSVC-specific construct, makes no 32-bit pointer assumption, and passes
`g++ -std=c++20 -fsyntax-only` with **zero errors**. It moved to GCC without a single edit, which
is what the charter's platform-neutral-core rule bought. The other side is the opposite: 270
`CONTEXT` uses, 47 `VirtualProtect`, 28 `fs:[...]` assembly sites, 15 `VirtualQuery`, 8
`VirtualAlloc`, three vectored handlers — the size of Stage 3.

## Judgement calls

**Conditional sources rather than a second library.** Splitting the target would have meant
rewiring every link and include relationship for no gain. While moving the sources, seven
`src/sound/*` files turned out to be interleaved among the Win32 ones; moving the block by line
range would have quietly made the sound core Windows-only.

**Probe membership had to be decided by compiling, not by grepping.** The first pass searched for
`windows.h` and `platform/win32` and produced fourteen "Win32-free" probes. That was wrong: probes
reach the Win32 layer through include directories with relative names like
`dos/dos_int21_services.h`, which no string search reveals. Compiling and linking each candidate
gave the real answer, nine. Two apparent link failures were also mine rather than the platform's —
a hand-written link line that omitted libchdr's LZMA dependency, which CMake supplies
automatically. Imitating the tool by hand forgets what the tool already knows.

**SDL3 needed its headless escape hatch.** SDL3 refuses to configure without X11 or Wayland
development packages. Nothing in Stage 1 opens a window and the core uses SDL only for the key
constants in `SDL_keycode.h`, so `-DSDL_UNIX_CONSOLE_BUILD=ON` costs nothing here; Stage 2 will
install the real desktop packages.

**The 32-bit toolchain is checked before configuring**, because its absence otherwise surfaces as
hundreds of header errors with the cause buried.

## The one portability defect found

`nvram_path_probe.cpp` used MSVC's `_putenv`; POSIX uses `setenv`/`unsetenv` and removes a
variable differently. It is now split by platform. Nothing of the kind appeared in the 53 core
sources — the defect was in a probe, which was never designed to be platform-neutral.

## Verification

`librepiu_exe.a` builds on WSL2 Ubuntu 24.04 with GCC 13.3, `repiu_core_probe` passes all nine
checks there, and Windows `repiu_aot_probe` still builds with zero errors, so the CMake
restructuring caused no regression. The same `repiu_core_probe` passes on Windows and the two runs
produce **byte-identical 46-line output**, which is the point of having one binary on both: the
claim is not that it passed but that the same code behaves the same on both systems. The Linux
artifact is an `ELF 32-bit LSB pie executable, Intel 80386`.

## Remaining

Stage 2 brings the launcher up under WSLg and needs i386 builds of SDL3 and ImGui with the 32-bit
desktop development packages. Stage 3 ports the execution engine: vectored handlers to POSIX
signals, `CONTEXT` to a rePIU-owned register structure, `VirtualProtect` to `mprotect`, and the
`fs:[...]` sites to Linux segments, with self-modifying-code detection as the central problem.
`repiu_chd_cd_probe` and `repiu_glide_issue_probe` were left unguarded because they reference no
Win32 symbol, but they have not yet been built on Linux.
