# spdlog loader 진단 출력 분리 설계

## 배경

현재 Win32 loader는 loader 진단 정보와 guest executable의 HLE console output을 모두 `std::cout`에 출력한다.
이 때문에 `dos4gw_hello` 같은 샘플의 실제 출력과 loader 내부 상태 로그가 같은 stream에 섞여 보인다.

실행 대상의 출력은 관찰 대상이고, loader 진단 정보는 host 도구 로그이다.
두 출력은 기본 채널부터 분리되어야 한다.

## 설계

Win32 loader의 진단 출력은 `spdlog`를 사용한다.
`spdlog` sink는 stderr color sink를 사용해 loader 로그를 stderr로 보낸다.
guest executable이 만든 HLE console output은 stdout에 그대로 쓴다.

loader 진단 코드는 stream buffer redirect나 `std::cout` 간접 호출을 사용하지 않는다.
각 진단 출력 지점은 코드에서 명확하게 `logger.info(...)` 또는 `logger.error(...)`를 호출한다.

초기 로그 형식은 다음 정보를 포함한다.

* log level
* logger name
* message

예:

```text
[info] [loader] Win32 loader target: piu_1st
```

guest output은 prefix 없이 stdout으로만 출력한다.

```text
Hello, world!
```

## 의존성

`spdlog`는 MIT 라이선스 기반 라이브러리이다.
프로젝트 정책상 GPL/LGPL/AGPL 계열이 아니므로 사용 가능하다.
초기 도입은 CMake `FetchContent`로 고정 버전을 받아 header-only target에 연결한다.

## 범위

이번 단계에서는 Win32 loader의 진단 출력만 `spdlog`로 옮긴다.
`repiu_exe_analyzer`는 별도 CLI 분석 도구이므로 출력 포맷 변경을 다음 작업으로 미룬다.

## 검증

* Win32 x86 loader를 빌드한다.
* `repiu_loader_win32.exe piu_1st`에서 loader 정보가 `spdlog` 형식으로 출력되는지 확인한다.
* `repiu_loader_win32.exe dos4gw_hello`에서 guest output `Hello, world!`가 prefix 없이 stdout에 유지되는지 확인한다.

# spdlog Loader Diagnostic Output Separation Design

## Background

The current Win32 loader prints loader diagnostics and guest executable HLE console output to `std::cout`.
This makes it hard to distinguish real sample output from loader internal state logs.

The target executable output is the observed program output, while loader diagnostics are host tool logs.
They should use separate default channels.

## Design

Win32 loader diagnostics use `spdlog`.
The `spdlog` sink is a stderr color sink so loader logs go to stderr.
Guest executable HLE console output remains written directly to stdout.

Loader diagnostic code does not use stream-buffer redirection or indirect `std::cout` calls.
Each diagnostic output site calls `logger.info(...)` or `logger.error(...)` explicitly.

The initial log format includes:

* log level
* logger name
* message

Example:

```text
[info] [loader] Win32 loader target: piu_1st
```

Guest output is emitted to stdout without a prefix.

```text
Hello, world!
```

## Dependency

`spdlog` is MIT licensed.
It is not GPL/LGPL/AGPL-family software, so it fits the project policy.
The initial integration uses CMake `FetchContent` with a pinned version and links the header-only target.

## Scope

This step moves only Win32 loader diagnostics to `spdlog`.
`repiu_exe_analyzer` is a separate CLI analysis tool, so changing its output format is deferred.

## Verification

* Build the Win32 x86 loader.
* Run `repiu_loader_win32.exe piu_1st` and confirm loader information is printed in the `spdlog` format.
* Run `repiu_loader_win32.exe dos4gw_hello` and confirm guest output `Hello, world!` remains unprefixed on stdout.
