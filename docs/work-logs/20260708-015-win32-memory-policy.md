# Win32/x86 Runtime Memory Policy 작업 로그

## 결과

runtime memory dry-run 결과를 기반으로 Win32/x86 직접 실행 가능성과 필요한 예약 주소 범위를 보고하는 정책 구조를 추가했다.

`Win32RuntimeMemoryPolicy`는 host pointer bit 수, direct x86 execution 지원 여부, preferred allocation base, required reserve size, HLE reserve base, 설명 메시지를 기록한다.

이번 단계에서는 실제 `VirtualAlloc`, 고정 주소 매핑, 원본 entry 호출을 추가하지 않았다.

## 변경 파일

* `CMakeLists.txt`
* `include/repiu/platform/win32/runtime_memory_policy.h`
* `src/platform/win32/runtime_memory_policy.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-015-win32-memory-policy.md`
* `docs/work-orders/20260708-015-win32-memory-policy.md`

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
Win32 runtime memory policy: valid
Win32 host pointer bits: 64
Win32 direct x86 execution: unsupported
Win32 preferred allocation base: 0x00010000
Win32 required reserve size: 0x005D7000
Win32 HLE reserve base: 0x005E7000
Win32 memory policy message: direct original x86 execution requires a 32-bit host process
Runtime memory dry run: valid
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

현재 검증 빌드는 64-bit host process이므로 direct x86 execution은 unsupported로 보고된다.

## 다음 단계

다음 단계에서는 Win32 x86 CMake 구성 또는 빌드 스크립트를 추가해 32-bit host process 빌드를 검증한다.

## Result

Added a policy structure that reports Win32/x86 direct execution capability and required reserve address range from the runtime memory dry-run result.

`Win32RuntimeMemoryPolicy` records host pointer bit count, direct x86 execution support, preferred allocation base, required reserve size, HLE reserve base, and an explanatory message.

This step does not add actual `VirtualAlloc`, fixed-address mapping, or original entry calls.

## Changed Files

* `CMakeLists.txt`
* `include/repiu/platform/win32/runtime_memory_policy.h`
* `src/platform/win32/runtime_memory_policy.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-015-win32-memory-policy.md`
* `docs/work-orders/20260708-015-win32-memory-policy.md`

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
Win32 runtime memory policy: valid
Win32 host pointer bits: 64
Win32 direct x86 execution: unsupported
Win32 preferred allocation base: 0x00010000
Win32 required reserve size: 0x005D7000
Win32 HLE reserve base: 0x005E7000
Win32 memory policy message: direct original x86 execution requires a 32-bit host process
Runtime memory dry run: valid
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

The current verification build is a 64-bit host process, so direct x86 execution is reported as unsupported.

## Next Step

The next step should add a Win32 x86 CMake configuration or build script and verify a 32-bit host process build.
