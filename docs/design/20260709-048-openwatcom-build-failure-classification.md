# OpenWatcom 샘플 빌드 실패 분류 설계

## 배경

OpenWatcom 로컬 샘플 스위트는 `clibexam`과 `cplbexam`의 모든 소스 파일을 안정적인 순서로 열거해 DOS/4GW console 대상 EXE로 빌드한다.
현재 manifest 기준 819개 중 31개가 빌드 실패로 기록된다.

이 실패에는 두 종류가 섞여 있다.

* DOS/4GW console 대상에 필요한 빌드 옵션이 빠진 샘플
* DOS/4GW flat 32-bit console 대상과 맞지 않거나, 설치된 샘플 파일 자체가 독립 컴파일 가능한 C/C++ 소스가 아닌 샘플

원본 OpenWatcom 샘플 소스는 프로젝트 라이선스 기준과 다르므로 저장소에 복사하거나 수정하지 않는다.
따라서 해결은 `scripts/build_openwatcom_samples.ps1`의 빌드 계획 계층에서 처리한다.

## 분류

빌드 옵션으로 해결 가능한 샘플:

* `clibexam/setnew.c`: `.c` 확장자지만 C++ `new` 문법을 사용하므로 `-cc++`로 C++ 컴파일을 강제한다.
* `cplbexam/contain/wcldintr.cpp`
* `cplbexam/contain/wcldptr.cpp`
* `cplbexam/contain/wcldval.cpp`
* `cplbexam/ios/except.cpp`
* 위 네 C++ 샘플은 예외 처리가 필요하므로 `-xs`를 추가한다.

명시적으로 skip할 샘플:

* based heap 전용: `_bfreese.c`, `_bheapse.c`
* default windowing 전용: `_dwdelcl.c`, `_dwshutd.c`, `_dwstabo.c`, `_dwstapt.c`, `_dwstcnt.c`, `_dwyield.c`
* 현재 DOS/4GW runtime library에 심볼이 없는 샘플: `_bthread.c`, `_clear87.c`, `_ethread.c`, `_expand.c`, `_fpreset.c`, `_pclose.c`, `_pipe.c`, `_popen.c`, `_status8.c`, `cwait.c`, `halloc.c`, `hfree.c`, `int86x.c`, `wait.c`
* 독립 컴파일 가능한 샘플 소스로 보기 어려운 파일: `lseek.c`, `strncoll.c`, `strnicol.c`, `strninc.c`

## 설계

`scripts/build_openwatcom_samples.ps1`에 샘플별 build plan 함수를 추가한다.

build plan은 다음을 제공한다.

* `Skip`: 현재 DOS/4GW console 대상에서 빌드를 시도하지 않을지 여부
* `SkipReason`: skip 이유
* `Options`: 기본 `-q -bt=dos -l=dos4g -fe=<exe>` 앞에 추가할 샘플별 컴파일 옵션

manifest에는 기존 필드를 유지하면서 다음 필드를 추가한다.

* `BuildSkipped`
* `BuildSkipReason`
* `BuildOptions`
* summary의 `BuildSkipped`

skip 샘플은 전체 샘플 수에는 포함하되 빌드 실패로 취급하지 않고 `BuildStatus = "skip"`으로 기록한다.
테스트 스크립트는 `skip`을 실행 대상에서 제외하고 report와 summary에 유지한다.
baseline 비교에서는 `skip`도 pass가 아닌 상태로 다루므로, 기존 pass 회귀를 숨기지 않는다.

검증 편의를 위해 빌드 스크립트에는 `-SkipHostBuild` 옵션을 둔다.
이 옵션은 이미 빌드된 loader를 유지한 채 OpenWatcom 샘플 manifest만 재생성한다.
잠겨 있는 loader 실행 파일 때문에 재링크가 불가능한 경우에도 샘플 빌드 계획을 검증할 수 있다.

## 검증

* `scripts/build_openwatcom_samples.ps1 -SkipSetup -SkipHostBuild`를 실행한다.
* manifest에서 빌드 실패가 0개인지 확인한다.
* 빌드 skip이 26개인지 확인한다.
* `scripts/test_openwatcom_samples.ps1 -CompareBaseline`을 실행한다.
* baseline regression이 0개인지 확인한다.

# OpenWatcom Sample Build Failure Classification Design

## Background

The OpenWatcom local sample suite enumerates every source file in `clibexam` and `cplbexam` in stable order and builds it as a DOS/4GW console EXE.
The current manifest records 31 build failures out of 819 samples.

Those failures contain two different categories.

* Samples missing build options required for the DOS/4GW console target.
* Samples that do not fit the DOS/4GW flat 32-bit console target, or installed sample files that are not standalone compilable C/C++ sources.

OpenWatcom sample sources use a license that differs from the project license baseline, so they are not copied into or modified in the repository.
The fix therefore belongs in the build-plan layer of `scripts/build_openwatcom_samples.ps1`.

## Classification

Samples fixable with build options:

* `clibexam/setnew.c`: the file has a `.c` extension but uses C++ `new` syntax, so force C++ compilation with `-cc++`.
* `cplbexam/contain/wcldintr.cpp`
* `cplbexam/contain/wcldptr.cpp`
* `cplbexam/contain/wcldval.cpp`
* `cplbexam/ios/except.cpp`
* The four C++ samples above require exception handling, so add `-xs`.

Samples to skip explicitly:

* based heap only: `_bfreese.c`, `_bheapse.c`
* default windowing only: `_dwdelcl.c`, `_dwshutd.c`, `_dwstabo.c`, `_dwstapt.c`, `_dwstcnt.c`, `_dwyield.c`
* symbols unavailable in the current DOS/4GW runtime library: `_bthread.c`, `_clear87.c`, `_ethread.c`, `_expand.c`, `_fpreset.c`, `_pclose.c`, `_pipe.c`, `_popen.c`, `_status8.c`, `cwait.c`, `halloc.c`, `hfree.c`, `int86x.c`, `wait.c`
* files that are not practical standalone sample sources: `lseek.c`, `strncoll.c`, `strnicol.c`, `strninc.c`

## Design

Add a per-sample build-plan function to `scripts/build_openwatcom_samples.ps1`.

The build plan provides:

* `Skip`: whether the sample should not be built for the current DOS/4GW console target.
* `SkipReason`: why it is skipped.
* `Options`: sample-specific compile options to add before the default `-q -bt=dos -l=dos4g -fe=<exe>` arguments.

The manifest keeps existing fields and adds:

* `BuildSkipped`
* `BuildSkipReason`
* `BuildOptions`
* `BuildSkipped` in the summary

Skipped samples remain part of the total sample count, but they are not treated as build failures and are recorded with `BuildStatus = "skip"`.
The test script excludes skipped samples from execution while keeping them visible in the report and summary.
Baseline comparison still treats `skip` as a non-pass state, so previous pass regressions are not hidden.

For verification convenience, the build script has a `-SkipHostBuild` option.
This option regenerates only the OpenWatcom sample manifest while keeping an already built loader.
It allows validating the sample build plan even when the loader executable is locked and cannot be relinked.

## Verification

* Run `scripts/build_openwatcom_samples.ps1 -SkipSetup -SkipHostBuild`.
* Confirm that the manifest has zero build failures.
* Confirm that the build skip count is 26.
* Run `scripts/test_openwatcom_samples.ps1 -CompareBaseline`.
* Confirm that baseline regression count is 0.
