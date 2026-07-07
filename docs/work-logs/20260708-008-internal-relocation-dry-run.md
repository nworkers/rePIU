# 작업 로그: 내부 Relocation Dry-Run

## 결과

완료.

디코딩된 내부 fixup record를 사용해 LE 이미지 버퍼에 relocation 값을 적용하는 dry-run을 추가했다.

source kind `0x07` record는 target object의 `relocation_base_address`와 `target_offset`을 더한 값을 32-bit little-endian으로 source 위치에 쓴다.

source kind `0x07`이 아닌 record와 현재 4KB page/object 버퍼 모델에서 직접 쓸 수 없는 source offset은 skipped로 집계한다.

분석 도구 출력에 relocation dry-run 유효성, applied/skipped/failed 수, unsupported source type 수, source out-of-range 수, 첫 적용 relocation 요약을 추가했다.

이번 단계에서는 실행 가능한 Win32 메모리나 selector/descriptor를 만들지 않았다.

## 검증

수행한 configure:

```text
cmake -S . -B build\vs2022_debug -G "Visual Studio 17 2022" -A x64
```

결과:

```text
Configuring done
Generating done
```

수행한 빌드:

```text
cmake --build build\vs2022_debug --config Debug
```

결과:

```text
repiu_exe_analyzer.vcxproj -> E:\MYWORK\Projects\rePIU\build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

수행한 분석:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

확인한 주요 출력:

```text
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
LE unsupported relocation source types: 9
LE source out-of-range relocations: 1
LE first applied relocation: source_object=2 source_offset=0x00000EC5 target_object=4 target_offset=0x00184B3D previous=0x00184B3D applied=0x002A4B3D
```

decoded fixup record 수는 14647이며, applied 14637 + skipped 10으로 전체 record 수와 일치한다.

## Work Log: Internal Relocation Dry-Run

## Result

Complete.

Added a dry-run that applies relocation values to LE image buffers using decoded internal fixup records.

For source kind `0x07`, the target object's `relocation_base_address` plus `target_offset` is written to the source location as a 32-bit little-endian value.

Records whose source kind is not `0x07`, and source offsets that cannot be written directly in the current 4 KB page/object buffer model, are counted as skipped.

Extended the analysis tool output with relocation dry-run validity, applied/skipped/failed counts, unsupported source type count, source out-of-range count, and the first applied relocation summary.

This step does not create executable Win32 memory or selectors/descriptors.

## Verification

Configure command:

```text
cmake -S . -B build\vs2022_debug -G "Visual Studio 17 2022" -A x64
```

Result:

```text
Configuring done
Generating done
```

Build command:

```text
cmake --build build\vs2022_debug --config Debug
```

Result:

```text
repiu_exe_analyzer.vcxproj -> E:\MYWORK\Projects\rePIU\build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

Analysis command:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

Key output confirmed:

```text
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
LE unsupported relocation source types: 9
LE source out-of-range relocations: 1
LE first applied relocation: source_object=2 source_offset=0x00000EC5 target_object=4 target_offset=0x00184B3D previous=0x00184B3D applied=0x002A4B3D
```

The decoded fixup record count is 14647, and applied 14637 + skipped 10 matches the full record count.
