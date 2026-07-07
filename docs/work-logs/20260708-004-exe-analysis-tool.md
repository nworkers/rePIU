# 작업 로그: PIU.EXE 비실행 분석 도구

## 결과

완료.

CMake 기반 최소 C++20 프로젝트 구조를 추가했다.

`include/repiu/exe/`, `src/exe/`, `src/tools/exe_analyzer/`를 추가하고, `repiu_exe_analyzer` 콘솔 도구를 구현했다.

도구는 `MASTER\PIU_1ST\PIU.EXE`를 실행하지 않고 읽어서 MZ 헤더와 LE 고정 헤더 정보를 출력한다.

`ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, `docs/EXE_DESIGN.en.md`에 이번 구조와 분석 범위를 반영했다.

## 검증

처음 실행한 CMake configure는 MSBuild 임시 파일 삭제 권한 문제로 실패했다. 같은 명령을 승인 실행으로 다시 수행해 성공했다.

수행한 configure:

```text
cmake -S . -B build\vs2022_debug -G "Visual Studio 17 2022" -A x64
```

수행한 빌드:

```text
cmake --build build\vs2022_debug --config Debug
```

빌드 결과:

```text
repiu_exe_analyzer.vcxproj -> E:\MYWORK\Projects\rePIU\build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

수행한 분석:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

확인한 주요 출력:

```text
MZ: valid
LE offset: 0x00002C90
LE signature: valid
LE CPU type: 0x0002 (80386)
LE object count: 4
LE page count: 392
LE entry object: 2
LE entry offset: 0x000E3818
```

## Work Log: PIU.EXE Non-Executing Analysis Tool

## Result

Complete.

Added a minimal CMake-based C++20 project structure.

Added `include/repiu/exe/`, `src/exe/`, and `src/tools/exe_analyzer/`, and implemented the `repiu_exe_analyzer` console tool.

The tool reads `MASTER\PIU_1ST\PIU.EXE` without executing it and prints MZ header and fixed LE header information.

Updated `ARCHITECTURE.md`, `docs/EXE_DESIGN.ko.md`, and `docs/EXE_DESIGN.en.md` with the new structure and analysis scope.

## Verification

The first CMake configure failed because MSBuild could not delete a temporary file under sandboxed execution. The same command succeeded when rerun with approval.

Configure command:

```text
cmake -S . -B build\vs2022_debug -G "Visual Studio 17 2022" -A x64
```

Build command:

```text
cmake --build build\vs2022_debug --config Debug
```

Build result:

```text
repiu_exe_analyzer.vcxproj -> E:\MYWORK\Projects\rePIU\build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

Analysis command:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

Key output confirmed:

```text
MZ: valid
LE offset: 0x00002C90
LE signature: valid
LE CPU type: 0x0002 (80386)
LE object count: 4
LE page count: 392
LE entry object: 2
LE entry offset: 0x000E3818
```
