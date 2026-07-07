# 작업 로그: Relocation Skipped Source 분석

## 결과

완료.

내부 relocation dry-run에서 skipped로 남은 record를 더 자세히 분석할 수 있도록 source kind별 count와 첫 skipped sample 출력을 추가했다.

`LeSkippedRelocation`을 추가해 skipped record의 page, record offset, source type, source kind, source offset, source object/offset, target object, target offset을 기록한다.

`LeRelocationDryRun`에 source kind count, 첫 unsupported source sample, 첫 source out-of-range sample을 추가했다.

이번 단계에서는 skipped relocation을 새로 적용하지 않았다.

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
LE relocation source kind counts: kind3=8 kind5=1 kind7=14638
LE first unsupported source relocation: page=227 record_offset=0x0001A05D source_type=0x0013 source_kind=0x0003 source_offset=0x0102 target_object=3 target_offset=0x00000004
LE first out-of-range relocation: page=227 record_offset=0x0001A2A1 source_type=0x0007 source_kind=0x0007 source_offset=0xFFFE source_object=2 source_object_offset=0x000F1FFE target_object=4 target_offset=0x00296730
```

기존 relocation dry-run 결과도 유지되었다.

```text
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

## Work Log: Relocation Skipped Source Analysis

## Result

Complete.

Added source-kind counts and first skipped sample output so skipped records from the internal relocation dry-run can be analyzed in more detail.

Added `LeSkippedRelocation` to record skipped record page, record offset, source type, source kind, source offset, source object/offset, target object, and target offset.

Added source kind counts, first unsupported source sample, and first source out-of-range sample to `LeRelocationDryRun`.

This step does not apply any additional skipped relocations.

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
LE relocation source kind counts: kind3=8 kind5=1 kind7=14638
LE first unsupported source relocation: page=227 record_offset=0x0001A05D source_type=0x0013 source_kind=0x0003 source_offset=0x0102 target_object=3 target_offset=0x00000004
LE first out-of-range relocation: page=227 record_offset=0x0001A2A1 source_type=0x0007 source_kind=0x0007 source_offset=0xFFFE source_object=2 source_object_offset=0x000F1FFE target_object=4 target_offset=0x00296730
```

The existing relocation dry-run result is preserved.

```text
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```
