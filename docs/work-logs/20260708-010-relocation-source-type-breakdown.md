# Relocation Source Type 상세 분석 작업 로그

## 결과

내부 relocation dry-run의 skipped source를 full source type 단위로 분리해 볼 수 있도록 분석 출력을 확장했다.

`LeRelocationDryRun`에 `source_type_counts`를 추가해 `source_type` 전체 값별 분포를 기록한다.

unsupported source에 대해 kind별 첫 skipped sample을 기록하고 출력한다.

이번 단계에서는 skipped relocation을 새로 적용하지 않았다.

## 변경 파일

* `include/repiu/exe/executable_headers.h`
* `src/exe/executable_headers.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-010-relocation-source-type-breakdown.md`
* `docs/work-orders/20260708-010-relocation-source-type-breakdown.md`

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
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

주요 출력:

```text
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
LE unsupported relocation source types: 9
LE source out-of-range relocations: 1
LE relocation source kind counts: kind3=8 kind5=1 kind7=14638
LE relocation source type counts: type0x0005=1 type0x0007=14638 type0x0013=8
LE first unsupported source kind 3: page=227 record_offset=0x0001A05D source_type=0x0013 source_offset=0x0102 target_object=3 target_offset=0x00000004
LE first unsupported source kind 5: page=239 record_offset=0x0001C15C source_type=0x0005 source_offset=0x0028 target_object=3 target_offset=0x00000032
LE first out-of-range relocation: page=227 record_offset=0x0001A2A1 source_type=0x0007 source_kind=0x0007 source_offset=0xFFFE source_object=2 source_object_offset=0x000F1FFE target_object=4 target_offset=0x00296730
```

기존 relocation dry-run 결과는 유지되었다.

## 다음 판단

`source_type=0x0013`은 kind `0x03`에 상위 비트가 결합된 형태이므로, kind만 보고 적용하지 않는다.

`source_type=0x0005`는 별도 sample이 확보되었으므로 다음 단계에서 16-bit offset write 가능성을 검토한다.

out-of-range `source_type=0x0007`, `source_offset=0xFFFE`는 현재 오브젝트 버퍼 경계를 넘는 위치이므로 runtime memory layout 설계와 함께 다룬다.

## Result

Extended the analysis output so skipped sources in the internal relocation dry-run can be separated by full source type.

Added `source_type_counts` to `LeRelocationDryRun` to record the distribution for every full `source_type` value.

Recorded and printed the first skipped sample for each unsupported source kind.

This step does not apply any additional skipped relocations.

## Changed Files

* `include/repiu/exe/executable_headers.h`
* `src/exe/executable_headers.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/EXE_DESIGN.ko.md`
* `docs/EXE_DESIGN.en.md`
* `docs/design/20260708-010-relocation-source-type-breakdown.md`
* `docs/work-orders/20260708-010-relocation-source-type-breakdown.md`

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
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

Key output:

```text
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
LE unsupported relocation source types: 9
LE source out-of-range relocations: 1
LE relocation source kind counts: kind3=8 kind5=1 kind7=14638
LE relocation source type counts: type0x0005=1 type0x0007=14638 type0x0013=8
LE first unsupported source kind 3: page=227 record_offset=0x0001A05D source_type=0x0013 source_offset=0x0102 target_object=3 target_offset=0x00000004
LE first unsupported source kind 5: page=239 record_offset=0x0001C15C source_type=0x0005 source_offset=0x0028 target_object=3 target_offset=0x00000032
LE first out-of-range relocation: page=227 record_offset=0x0001A2A1 source_type=0x0007 source_kind=0x0007 source_offset=0xFFFE source_object=2 source_object_offset=0x000F1FFE target_object=4 target_offset=0x00296730
```

The existing relocation dry-run result is preserved.

## Next Judgment

`source_type=0x0013` combines kind `0x03` with a high bit, so it should not be applied from kind alone.

`source_type=0x0005` now has a separate sample and can be reviewed in the next step for possible 16-bit offset write handling.

The out-of-range `source_type=0x0007`, `source_offset=0xFFFE` crosses the current object buffer boundary, so it should be handled together with runtime memory layout design.
