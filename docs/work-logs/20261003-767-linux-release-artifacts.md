# 작업 로그 767: GitHub 릴리스에 Linux i386·x64 포함

설계: [20261003-767-linux-release-artifacts.md](../design/20261003-767-linux-release-artifacts.md) ·
작업 지시: [20261003-767-linux-release-artifacts.md](../work-orders/20261003-767-linux-release-artifacts.md)

## 수행 내용

* `CMakeLists.txt`: `REPIU_STATIC_RUNTIME` 옵션(기본 OFF). 켜면 `-static-libstdc++ -static-libgcc`.
* `build_linux_i386.sh`, `build_linux_x64.sh`: `--static-runtime`. 콘솔 스위치처럼 ON/OFF를 매번
  넘겨 이전 실행의 캐시 값이 남지 않게 했다. 두 스크립트에 실행 권한(755)을 줬다.
* `scripts/package_release_linux.sh` 신규. 바이너리 다섯 개를 strip하고 문서 다섯 개와 함께
  `rePIU-v<version>-linux-<arch>.tar.gz`로 묶는다(소유자 0/0).
* `release.yml`: `version`(ubuntu, bash로 이식) → `win32`(기존 단계, 게시 제외) ∥ `linux`(matrix i386,
  x64) → `publish`(아티팩트 4종이 모두 있어야 Release 생성·첨부). Linux job은 SDL 드라이버
  다섯 가지(X11, Wayland, libdecor, PulseAudio, ALSA), `libstdc++` 동적 링크 여부, glibc 2.35 상한을
  검사하고 probe 두 개를 돌린다.
* `docs/guides/release-and-ci.md`, `ARCHITECTURE.md`: Linux 아티팩트, 실행 조건(32비트 런타임 패키지
  목록), 로컬 재현, 러너 고정 이유.

## 검증

* 로컬(Ubuntu 25.10, x64): `scripts/build_linux_x64.sh --config Release --static-runtime
  --build-dir build/linux_x64_release` 성공.
  * SDL 기능 다섯 개 모두 확인.
  * `repiu`, `repiu_launcher`의 NEEDED는 `libGL.so.1`, `libm.so.6`, `libc.so.6`, `ld-linux-x86-64.so.2`이며
    `libstdc++`는 없다.
  * glibc 요구는 `GLIBC_2.43`. 로컬 배포판의 glibc를 따르므로 예상대로이고, 2.35 상한 검사는 CI 러너에서만 의미가 있다.
  * `repiu_core_probe`, `repiu_glide_issue_probe`는 `DISPLAY`·`WAYLAND_DISPLAY` 없이 실행해도 0으로 끝난다.
* 패키지에서 꺼낸 strip된 `repiu`로 pumpit1을 15초씩 실행했다. 게임은 돌았지만 timeout 종료 경로에서
  종료 코드가 0, 3, 139(SIGSEGV)로 달랐다. strip하지 않은 Release 바이너리도 같은 SIGSEGV를 냈으므로
  패키징 때문은 아니다. [linux-port-frontier.md](../analysis/linux-port-frontier.md)에 기록된 timeout
  teardown segfault("`step=done` 전에서 여전히 발생")와 같은 증상이며, 이 작업에서는 다루지 않았다.
* i386: 이 머신에 32비트 툴체인이 없어 로컬 빌드를 하지 못했다. `release.yml`의 YAML 구조만
  파싱으로 확인했다. 워크플로 자체(i386 패키지 설치, 22.04 빌드, publish)는 **아직 실행하지 않았다.**
  브랜치를 push한 뒤 `workflow_dispatch`로 확인해야 한다.

### CI 수동 실행 (run 37105245365, 브랜치 `linux-release-artifacts`)

* `version`, `win32`, `linux i386`, `linux x64`가 모두 성공했다. `publish`는 수동 실행이라 설계대로 건너뛰었다.
* 두 Linux job: SDL 기능 다섯 개 확인. `repiu`, `repiu_launcher`의 NEEDED는 `libGL`, `libm`, `libc`,
  로더뿐이고 glibc 요구는 `GLIBC_2.34`(상한 2.35 이하). `repiu_core_probe`의 모든 `*_all=true`.
* 아티팩트: `rePIU-v0.0.198-linux-x64`(10.4 MB), `…-linux-i386`(10.8 MB), Win32 `rePIU-v0.0.198`(10.5 MB).
* CI가 만든 x64 아카이브의 `repiu`를 로컬에서 pumpit1로 세 번 실행했다(15초씩). 매번 게임이 돌았다(1,089~1,370
  프레임). 종료 코드는 timeout 종료 경로에서 139, 0, 3으로, 위에 적은 기존 teardown 문제와 같다.
* 덧붙여, 원격에 push된 `v0.0.198` 태그 실행(run 37047800594)은 빌드와 샘플이 모두 통과했지만 기존 PowerShell
  게시 단계에서 실패해 Release가 만들어지지 않았다. 릴리스 노트 파일이 없어 `--generate-notes` 경로를 탔고, 이때
  `gh`가 `no matches found for '-'`로 끝났다. 이번 작업의 bash `publish` job이 그 단계를 대체한다.

---

# Work Log 767: Linux i386 and x64 in the GitHub release

Design: [20261003-767-linux-release-artifacts.md](../design/20261003-767-linux-release-artifacts.md) ·
Work order: [20261003-767-linux-release-artifacts.md](../work-orders/20261003-767-linux-release-artifacts.md)

## Changes

* `CMakeLists.txt`: the `REPIU_STATIC_RUNTIME` option (default OFF), adding
  `-static-libstdc++ -static-libgcc`.
* `build_linux_i386.sh`, `build_linux_x64.sh`: `--static-runtime`, passed ON or OFF on every run
  like the console switch so a cached answer does not linger. Both scripts are now executable (755).
* New `scripts/package_release_linux.sh`: strips the five binaries and packs them with five
  documents into `rePIU-v<version>-linux-<arch>.tar.gz` (owner 0/0).
* `release.yml`: `version` (ubuntu, ported to bash) → `win32` (the existing steps without
  publishing) ∥ `linux` (matrix i386, x64) → `publish` (creates or attaches the release only when
  all four archives are present). The Linux job checks SDL's five drivers (X11, Wayland, libdecor,
  PulseAudio, ALSA), that `libstdc++` is not linked dynamically and the glibc 2.35 ceiling, and
  runs two probes.
* `docs/guides/release-and-ci.md`, `ARCHITECTURE.md`: the Linux artifacts, what running them
  needs (with the 32-bit runtime package list), reproducing them locally, and why the runner is pinned.

## Verification

* Local (Ubuntu 25.10, x64): `scripts/build_linux_x64.sh --config Release --static-runtime
  --build-dir build/linux_x64_release` succeeds.
  * All five SDL features are present.
  * `repiu` and `repiu_launcher` need `libGL.so.1`, `libm.so.6`, `libc.so.6` and
    `ld-linux-x86-64.so.2`, with no `libstdc++`.
  * They need `GLIBC_2.43`, as expected for this distribution; the 2.35 ceiling check only means
    something on the CI runner.
  * `repiu_core_probe` and `repiu_glide_issue_probe` exit 0 with neither `DISPLAY` nor
    `WAYLAND_DISPLAY` set.
* The stripped `repiu` from the archive ran pumpit1 for 15 seconds a time. The game ran, but on
  the timeout exit path the exit status varied between 0, 3 and 139 (SIGSEGV). The unstripped
  Release binary gave the same SIGSEGV, so packaging is not the cause. It matches the timeout
  teardown segfault recorded in [linux-port-frontier.md](../analysis/linux-port-frontier.md)
  ("remains before `step=done`") and is not addressed here.
* i386: this machine has no 32-bit toolchain, so nothing was built locally. Only the YAML
  structure of `release.yml` was checked, by parsing it. The workflow itself (installing the i386
  packages, building on 22.04, publishing) **has not run yet**; it needs the branch pushed and a
  `workflow_dispatch` run.

### Manual CI run (run 37105245365, branch `linux-release-artifacts`)

* `version`, `win32`, `linux i386` and `linux x64` all succeeded; `publish` was skipped, as
  designed for a manual run.
* Both Linux jobs: all five SDL features present; `repiu` and `repiu_launcher` need only
  `libGL`, `libm`, `libc` and the loader, and `GLIBC_2.34` (within the 2.35 ceiling); every
  `*_all=true` in `repiu_core_probe`.
* Artifacts: `rePIU-v0.0.198-linux-x64` (10.4 MB), `…-linux-i386` (10.8 MB), Win32
  `rePIU-v0.0.198` (10.5 MB).
* The CI-built x64 `repiu` ran pumpit1 locally three times (15 seconds each). The game ran every
  time (1,089 to 1,370 frames); the exit status on the timeout path was 139, 0 and 3, the existing
  teardown problem noted above.
* Also, the pushed `v0.0.198` tag run (run 37047800594) passed its build and samples but failed in
  the old PowerShell publish step, so no release was created. With no release-notes file it took
  the `--generate-notes` path, and `gh` ended with `no matches found for '-'`. This task's bash
  `publish` job replaces that step.
