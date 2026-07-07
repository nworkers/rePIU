# DOS/4GW Loader Result 작업 로그

## 결과

기존 analyzer의 MZ/LE/image/fixup/relocation 순차 호출을 `LoadDos4gwExecutable`로 묶었다.

`Dos4gwLoadResult`는 MZ 헤더, LE 헤더, 매핑된 LE 이미지, fixup section 분석 결과, fixup record 디코딩 결과, relocation dry-run 결과를 포함한다.

`repiu_exe_analyzer`는 이제 개별 parser 함수를 직접 순서대로 호출하지 않고 `Dos4gwLoadResult`를 출력한다.

이번 단계에서는 원본 코드 실행, 실행 메모리 할당, HLE 호출은 추가하지 않았다.

## 변경 파일

* `CMakeLists.txt`
* `include/repiu/exe/dos4gw_loader.h`
* `src/exe/dos4gw_loader.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-013-dos4gw-loader-result.md`
* `docs/work-orders/20260708-013-dos4gw-loader-result.md`

## 검증

Debug 빌드:

```text
cmake --build build\vs2022_debug --config Debug
```

결과:

```text
repiu_exe.lib 빌드 성공
repiu_exe_analyzer.exe 빌드 성공
```

분석 도구 실행:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

주요 출력:

```text
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
LE relocation source type counts: type0x0005=1 type0x0007=14638 type0x0013=8
```

기존 분석 결과가 유지되었다.

## 다음 단계

다음 단계에서는 `Dos4gwLoadResult`를 입력으로 받는 runtime memory dry-run 구조를 설계한다.

## Result

Grouped the analyzer's existing MZ/LE/image/fixup/relocation sequence into `LoadDos4gwExecutable`.

`Dos4gwLoadResult` contains the MZ header, LE header, mapped LE image, fixup section analysis result, fixup record decoding result, and relocation dry-run result.

`repiu_exe_analyzer` now prints a `Dos4gwLoadResult` instead of calling each parser function directly in sequence.

This step does not add original-code execution, executable memory allocation, or HLE calls.

## Changed Files

* `CMakeLists.txt`
* `include/repiu/exe/dos4gw_loader.h`
* `src/exe/dos4gw_loader.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-013-dos4gw-loader-result.md`
* `docs/work-orders/20260708-013-dos4gw-loader-result.md`

## Verification

Debug build:

```text
cmake --build build\vs2022_debug --config Debug
```

Result:

```text
repiu_exe.lib build succeeded
repiu_exe_analyzer.exe build succeeded
```

Analyzer run:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

Key output:

```text
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
LE relocation source type counts: type0x0005=1 type0x0007=14638 type0x0013=8
```

The existing analysis result is preserved.

## Next Step

The next step should design a runtime memory dry-run structure that accepts `Dos4gwLoadResult` as input.
