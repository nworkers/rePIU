# PIU.EXE 비실행 분석 도구 설계

## 배경

`MASTER\PIU_1ST\PIU.EXE`를 바로 실행하기 전에, 원본 실행 파일의 MZ/LE 구조를 안정적으로 읽고 기록하는 도구가 필요하다.

이 도구는 원본 코드를 실행하지 않는다. 파일을 읽어 구조를 분석하고, 이후 LE 이미지 매핑과 HLE 런타임 구현의 기준 데이터를 제공한다.

## 목표

첫 번째 구현은 `PIU.EXE`에서 다음 정보를 출력한다.

* 타깃 ID와 실행 파일 경로
* 파일 크기
* MZ 헤더 유효성
* LE 헤더 오프셋과 시그니처
* LE byte order, word order, CPU type, OS type, module flags
* 페이지 수, 엔트리 오브젝트, 엔트리 오프셋, 스택 오브젝트, 스택 오프셋
* 오브젝트 테이블 오프셋과 개수
* 페이지 테이블 오프셋
* fixup page table과 fixup record table 오프셋
* data pages 오프셋

## 구조

분석 도구는 플랫폼 공용 코드로 시작한다.

* `include/repiu/exe/`: 실행 파일 분석용 공용 헤더
* `src/exe/`: MZ/LE 파서 구현
* `src/tools/exe_analyzer/`: 콘솔 분석 도구 진입점

초기에는 Win32 전용 API를 사용하지 않는다. 파일 입출력은 C++ 표준 라이브러리를 사용한다.

## 파서 책임

`MzParser`는 DOS MZ 헤더의 최소 필드와 `e_lfanew`만 해석한다.

`LeParser`는 LE 헤더의 고정 헤더 필드를 little-endian으로 읽고, 테이블의 실제 상세 파싱은 다음 작업으로 미룬다.

이번 단계에서는 오브젝트/페이지/fixup 테이블의 위치와 개수를 출력하는 데 집중한다.

## 검증

검증은 다음 명령으로 수행한다.

```text
cmake -S . -B build\vs2022_debug -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022_debug --config Debug
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

도구는 `LE offset: 0x00002C90`과 `LE signature: valid`를 출력해야 한다.

## Background

Before executing `MASTER\PIU_1ST\PIU.EXE`, the project needs a tool that reliably reads and records the original executable's MZ/LE structure.

This tool does not execute original code. It reads the file, analyzes its structure, and provides baseline data for later LE image mapping and HLE runtime work.

## Goal

The first implementation prints these values from `PIU.EXE`:

* target ID and executable path
* file size
* MZ header validity
* LE header offset and signature
* LE byte order, word order, CPU type, OS type, module flags
* page count, entry object, entry offset, stack object, stack offset
* object table offset and count
* page table offset
* fixup page table and fixup record table offsets
* data pages offset

## Structure

The analysis tool starts as platform-neutral code.

* `include/repiu/exe/`: public headers for executable analysis
* `src/exe/`: MZ/LE parser implementation
* `src/tools/exe_analyzer/`: console analysis tool entry point

The initial implementation does not use Win32-specific APIs. File I/O uses the C++ standard library.

## Parser Responsibilities

`MzParser` interprets only the minimum DOS MZ header fields and `e_lfanew`.

`LeParser` reads fixed LE header fields as little-endian values. Detailed parsing of tables is deferred to later work.

This step focuses on reporting object/page/fixup table locations and counts.

## Verification

Verification is performed with:

```text
cmake -S . -B build\vs2022_debug -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022_debug --config Debug
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

The tool must print `LE offset: 0x00002C90` and `LE signature: valid`.
