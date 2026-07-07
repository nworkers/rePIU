# Runtime Memory Dry-Run 작업 로그

## 결과

`Dos4gwLoadResult`를 입력으로 받아 실행 전 runtime memory 배치 계획을 계산하는 dry-run을 추가했다.

`RuntimeMemoryPlan`은 object region 목록, entry linear address, stack top linear address, HLE reserve base, total object virtual bytes를 기록한다.

이번 단계에서는 실제 실행 메모리 할당, selector/GDT 구성, 원본 코드 실행을 추가하지 않았다.

## 변경 파일

* `CMakeLists.txt`
* `include/repiu/runtime/runtime_memory.h`
* `src/runtime/runtime_memory.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-014-runtime-memory-dry-run.md`
* `docs/work-orders/20260708-014-runtime-memory-dry-run.md`

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
Runtime memory dry run: valid
Runtime object regions: 4
Runtime total object virtual bytes: 5983128
Runtime entry: 0x00103818
Runtime stack top: 0x005E6E10
Runtime HLE reserve base: 0x005E7000
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

## 다음 단계

다음 단계에서는 Win32/x86 실행 메모리 할당 전제와 selector abstraction 설계를 진행한다.

## Result

Added a dry-run that calculates a pre-execution runtime memory layout plan from `Dos4gwLoadResult`.

`RuntimeMemoryPlan` records object regions, entry linear address, stack top linear address, HLE reserve base, and total object virtual bytes.

This step does not add actual executable memory allocation, selector/GDT setup, or original-code execution.

## Changed Files

* `CMakeLists.txt`
* `include/repiu/runtime/runtime_memory.h`
* `src/runtime/runtime_memory.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-014-runtime-memory-dry-run.md`
* `docs/work-orders/20260708-014-runtime-memory-dry-run.md`

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
Runtime memory dry run: valid
Runtime object regions: 4
Runtime total object virtual bytes: 5983128
Runtime entry: 0x00103818
Runtime stack top: 0x005E6E10
Runtime HLE reserve base: 0x005E7000
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

## Next Step

The next step should design Win32/x86 executable memory allocation assumptions and selector abstraction.
