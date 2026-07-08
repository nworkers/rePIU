# OpenWatcom DOS/4GW Hello 샘플 설계

## 배경

loader의 주 실행 대상은 원본 `PIU.EXE`이며, 원본 entry 진입 후 `0x020F3890`에서 privileged instruction 예외를 관찰한다.
원본 게임을 곧바로 진행시키려면 DOS/4GW 초기화, DPMI, DOS INT 처리 같은 여러 계약을 동시에 해석해야 한다.

테스트를 단순화하기 위해 OpenWatcom으로 실제 DOS/4G runtime에서도 실행 가능한 DOS/4GW `Hello, world!` 샘플을 둔다.
샘플은 원본 게임 로직을 대체하지 않는다.
loader/HLE 계약을 좁은 입력으로 검증하기 위한 보조 target이다.

## 라이선스 및 도구 배치

OpenWatcom v2는 GitHub `open-watcom/open-watcom-v2` 프로젝트에서 배포하며, release 페이지에는 `Current-build` asset이 제공된다.
프로젝트 라이선스는 Sybase Open Watcom Public License 1.0 계열이다.

해당 라이선스는 프로젝트 기본 BSD 3-Clause 기준과 다르므로 OpenWatcom 자체는 저장소에 vendoring하지 않는다.
로컬 설치물은 `tools/openwatcom/` 아래에 두고 `.gitignore`로 제외한다.
저장소에는 다운로드/설치 스크립트, 샘플 소스, 빌드 스크립트, 문서만 포함한다.

## 목표

* `samples/dos4gw_hello/hello.c`에 표준 C `main`/`puts` 기반 `Hello, world!` 샘플을 둔다.
* 빌드 스크립트는 `wcl386 -bt=dos -l=dos4g`로 DOS/4G runtime 호환 executable을 생성한다.
* 샘플 executable은 DOS/4G가 요구하는 LE stack object를 포함해야 한다.
* `dos4gw_hello` target profile은 `samples/dos4gw_hello/build/hello.exe`를 가리킨다.
* loader는 샘플 target에서 OpenWatcom C runtime startup과 console 출력에 필요한 최소 DOS/DPMI HLE를 처리한다.
* 기존 `piu_1st` target의 관찰 지점은 유지한다.

## 비목표

* OpenWatcom 바이너리나 소스 코드를 Git에 커밋하지 않는다.
* 원본 `PIU.EXE` 로딩 경로를 제거하지 않는다.
* 전체 DOS/DPMI API를 구현하지 않는다.
* 게임 출력이나 그래픽을 목표로 하지 않는다.

## 설계 결정

### Tool Layout

```text
tools/
  openwatcom/        # 로컬 설치물, Git 제외
  downloads/         # 다운로드 cache, Git 제외
```

### Sample Layout

```text
samples/dos4gw_hello/
  hello.c
  build/             # 빌드 산출물, Git 제외
```

`hello.c`는 표준 C runtime entry를 사용한다.
custom `_start`, `int 3` trap, stack object가 없는 executable은 실제 DOS/4G runtime 샘플로 쓰지 않는다.

### Loader HLE Scope

`dos4gw_hello` target에서만 DOS console HLE 실행 경로를 켠다.
OpenWatcom C runtime 샘플 진행에 필요한 범위만 처리하고, 지원하지 않는 interrupt나 함수는 관찰 가능한 예외/메시지로 남긴다.

## 검증

* `cmd /c scripts\build_dos4gw_hello.bat`가 `hello.exe`를 생성한다.
* OpenWatcom `wdump`와 analyzer에서 LE stack object가 유효해야 한다.
* loader가 `dos4gw_hello` target에서 `Hello, world!`를 host stdout에 출력한다.
* loader가 `piu_1st` target에서 기존 `0x020F3890` privileged instruction 예외 관찰을 유지한다.

# OpenWatcom DOS/4GW Hello Sample Design

## Background

The loader's primary execution target is the original `PIU.EXE`, and it observes a privileged-instruction exception at `0x020F3890` after entering the original entry.
Progressing the original game immediately requires interpreting several contracts at once: DOS/4GW initialization, DPMI, and DOS INT handling.

To simplify testing, this task keeps a DOS/4GW `Hello, world!` sample built with OpenWatcom that is also valid for the real DOS/4G runtime.
The sample does not replace original game logic.
It is only a helper target for validating loader/HLE contracts with a narrow input.

## License And Tool Placement

OpenWatcom v2 is distributed from the GitHub `open-watcom/open-watcom-v2` project, whose release page provides `Current-build` assets.
The project uses the Sybase Open Watcom Public License 1.0 family.

Because that license differs from the project's BSD 3-Clause baseline, OpenWatcom itself is not vendored into this repository.
The local installation lives under `tools/openwatcom/` and is excluded by `.gitignore`.
Only download/install scripts, sample source, build scripts, and documentation are committed.

## Goals

* Keep the `Hello, world!` sample as standard C `main`/`puts` source at `samples/dos4gw_hello/hello.c`.
* Build a DOS/4G-runtime-compatible executable with `wcl386 -bt=dos -l=dos4g`.
* Ensure the sample executable includes the LE stack object required by DOS/4G.
* Point the `dos4gw_hello` target profile to `samples/dos4gw_hello/build/hello.exe`.
* Let the loader handle the minimal DOS/DPMI HLE required by OpenWatcom C runtime startup and console output for the sample target.
* Preserve the existing `piu_1st` target observation point.

## Non-Goals

* Do not commit OpenWatcom binaries or source code to Git.
* Do not remove the original `PIU.EXE` loading path.
* Do not implement the full DOS/DPMI API.
* Do not target game output or graphics.

## Design Decisions

### Tool Layout

```text
tools/
  openwatcom/        # local install, excluded from Git
  downloads/         # download cache, excluded from Git
```

### Sample Layout

```text
samples/dos4gw_hello/
  hello.c
  build/             # build outputs, excluded from Git
```

`hello.c` uses the standard C runtime entry.
Custom `_start`, `int 3` traps, and executables without stack objects are not used as real DOS/4G runtime samples.

### Loader HLE Scope

The DOS console HLE execution path is enabled only for the `dos4gw_hello` target.
It handles only the scope required to progress the OpenWatcom C runtime sample, while unsupported interrupts or functions remain observable exceptions/messages.

## Verification

* `cmd /c scripts\build_dos4gw_hello.bat` generates `hello.exe`.
* OpenWatcom `wdump` and the analyzer report a valid LE stack object.
* The loader prints `Hello, world!` to host stdout for the `dos4gw_hello` target.
* The loader preserves the existing `0x020F3890` privileged-instruction observation for the `piu_1st` target.
