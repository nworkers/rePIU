# Relocation Source Type 상세 분석 설계

## 배경

이전 단계에서 skipped relocation은 `kind3=8`, `kind5=1`, `kind7=14638`로 관찰되었다.

하지만 `kind3`의 첫 샘플은 `source_type=0x13`이므로 lower nibble만으로 의미를 확정하면 안 된다.

source type 전체 값과 unsupported kind별 첫 샘플을 출력해, source kind와 source flag를 분리해서 판단한다.

## 목표

이번 단계는 relocation 적용 범위를 늘리지 않고 분석 출력을 확장한다.

* source type 전체 값별 count를 출력한다.
* unsupported source kind별 첫 record를 출력한다.
* 기존 source kind count, 첫 unsupported sample, 첫 out-of-range sample 출력을 유지한다.

## 비목표

* `source_type=0x13` relocation 적용
* `source kind 0x05` relocation 적용
* `source_offset=0xFFFE` out-of-range relocation 적용
* 원본 코드 실행

## 검증 기준

분석 도구 출력에서 다음을 확인한다.

* `LE relocation source type counts:`가 출력된다.
* `LE first unsupported source kind 3:`이 출력된다.
* `LE first unsupported source kind 5:`가 출력된다.
* 기존 relocation dry-run 결과가 유지된다.

## 다음 단계

출력된 source type 분포를 기준으로 `source_type=0x13`의 상위 비트 의미를 문서화하고, `source kind 0x05`가 16-bit offset write로 안전하게 처리 가능한지 별도 설계한다.

## Background

The previous step observed skipped relocations as `kind3=8`, `kind5=1`, and `kind7=14638`.

However, the first `kind3` sample has `source_type=0x13`, so the meaning must not be finalized from the lower nibble alone.

The analyzer prints counts by full source type and the first unsupported record for each source kind so source kinds and source flags can be judged separately.

## Goal

This step expands analysis output without increasing relocation application coverage.

* Print counts by full source type.
* Print the first unsupported record for each unsupported source kind.
* Preserve the existing source kind counts, first unsupported sample, and first out-of-range sample.

## Non-Goals

* Applying `source_type=0x13` relocations.
* Applying `source kind 0x05` relocations.
* Applying the `source_offset=0xFFFE` out-of-range relocation.
* Executing original code.

## Verification Criteria

The analysis tool output must include:

* `LE relocation source type counts:`
* `LE first unsupported source kind 3:`
* `LE first unsupported source kind 5:`
* unchanged existing relocation dry-run results

## Next Step

Use the printed source type distribution to document the high-bit meaning of `source_type=0x13`, then separately design whether `source kind 0x05` can safely be handled as a 16-bit offset write.
