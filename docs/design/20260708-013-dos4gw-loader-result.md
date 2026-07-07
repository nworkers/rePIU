# DOS/4GW Loader Result 설계

## 배경

현재 `repiu_exe_analyzer`는 MZ 파싱, LE 파싱, 이미지 매핑, fixup 분석, relocation dry-run을 순서대로 직접 호출한다.

향후 runtime도 같은 로딩 단계를 사용해야 하므로, 이 흐름을 공용 `Dos4gwExecutableLoader` 단위로 묶어야 한다.

이번 단계는 원본 코드 실행이나 runtime memory 배치가 아니라, 기존 분석 흐름을 재사용 가능한 결과 구조로 정리하는 것이다.

## 목표

* `Dos4gwLoadResult` 구조를 추가한다.
* `LoadDos4gwExecutable` 함수를 추가한다.
* analyzer가 개별 parser 함수를 직접 나열하지 않고 loader result를 사용하게 한다.
* 기존 출력과 분석 결과를 유지한다.

## 비목표

* 원본 코드 실행
* 실행 가능한 메모리 할당
* DPMI/DOS HLE 호출
* relocation semantics 확장
* 동적 플러그인 시스템

## 설계

`Dos4gwLoadResult`는 다음 값을 가진다.

* `MzHeader`
* `LeHeader`
* `LeImage`
* `LeFixupInfo`
* `LeFixupRecordInfo`
* `LeRelocationDryRun`

`LoadDos4gwExecutable`은 파일 bytes와 target profile을 받아 기존 순서대로 로딩 dry-run을 수행한다.

실패 시 기존 `ParseError`를 반환하며, 성공 시 모든 분석 결과를 하나의 result에 채운다.

## 검증 기준

* Debug 빌드가 성공한다.
* analyzer 출력의 기존 핵심 수치가 유지된다.
* `LE relocation dry run: valid`
* `LE applied relocations: 14637`
* `LE failed relocations: 0`
* `LE skipped relocations: 10`

## 향후 확장

다음 단계에서 `Dos4gwLoadResult`를 runtime memory 준비 입력으로 사용한다.

추가 target이 생기면 같은 loader result 구조를 통해 분석과 실행 준비를 공유한다.

## Background

`repiu_exe_analyzer` currently calls MZ parsing, LE parsing, image mapping, fixup analysis, and relocation dry-run directly in sequence.

The future runtime must use the same loading steps, so this flow should be grouped into a shared `Dos4gwExecutableLoader` unit.

This step does not execute original code or place runtime memory. It organizes the existing analysis flow into a reusable result structure.

## Goal

* Add the `Dos4gwLoadResult` structure.
* Add the `LoadDos4gwExecutable` function.
* Make the analyzer use the loader result instead of spelling out each parser call.
* Preserve existing output and analysis results.

## Non-Goals

* Executing original code.
* Allocating executable memory.
* Calling DPMI/DOS HLE.
* Expanding relocation semantics.
* Dynamic plugin system.

## Design

`Dos4gwLoadResult` contains:

* `MzHeader`
* `LeHeader`
* `LeImage`
* `LeFixupInfo`
* `LeFixupRecordInfo`
* `LeRelocationDryRun`

`LoadDos4gwExecutable` receives file bytes and a target profile, then performs the existing loading dry-run sequence.

On failure it returns the existing `ParseError`; on success it fills all analysis results into one result.

## Verification Criteria

* Debug build succeeds.
* Existing key analyzer numbers are preserved.
* `LE relocation dry run: valid`
* `LE applied relocations: 14637`
* `LE failed relocations: 0`
* `LE skipped relocations: 10`

## Future Extension

A later step will use `Dos4gwLoadResult` as input to runtime memory preparation.

When additional targets are added, analysis and execution preparation will share the same loader result structure.
