# LE 이미지 매핑 Dry-Run 설계

## 배경

비실행 분석 도구로 `MASTER\PIU_1ST\PIU.EXE`의 MZ/LE 고정 헤더를 확인했다.

다음 단계는 원본 코드를 실행하지 않고 LE 오브젝트 테이블과 페이지 테이블을 해석해, 보호 모드 코드가 기대하는 메모리 이미지의 초안을 만드는 것이다.

## 목표

이번 단계는 다음을 구현한다.

* LE 오브젝트 테이블 파싱
* LE 페이지 테이블 파싱
* 오브젝트별 가상 메모리 버퍼 생성
* 페이지 테이블을 따라 원본 파일의 data page를 오브젝트 버퍼에 복사
* 엔트리 포인트가 어떤 오브젝트와 오프셋에 위치하는지 검증
* 분석 도구 출력에 오브젝트/페이지/이미지 매핑 요약 추가

## 비목표

* 원본 코드 실행
* fixup record 상세 파싱
* relocation 적용
* selector/descriptor 런타임 구현
* Win32 실행 전환

## LE 테이블 해석

현재 `PIU.EXE`에서 확인한 LE 오브젝트 테이블은 24바이트 레코드로 해석한다.

* `virtual_size`
* `relocation_base_address`
* `flags`
* `page_table_index`
* `page_count`
* `reserved`

LE 페이지 테이블은 4바이트 레코드로 해석한다.

* 앞 3바이트: big-endian 형태의 data page 번호
* 마지막 1바이트: page flags

data page 번호가 1이면 파일의 `data_pages_offset` 위치를 가리키며, N이면 `data_pages_offset + (N - 1) * page_size`를 가리킨다.

## 출력

`repiu_exe_analyzer`는 기존 헤더 출력에 더해 다음을 출력한다.

* 오브젝트별 가상 크기, flags, page range
* 페이지 테이블 첫/마지막 레코드 요약
* 매핑된 오브젝트 수
* 총 가상 메모리 크기
* 총 파일에서 복사한 바이트 수
* 엔트리 포인트 검증 결과

## 검증

검증은 기존 빌드 절차 후 다음 명령으로 수행한다.

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

기대 출력:

* `LE image map: valid`
* `LE mapped objects: 4`
* `LE entry mapping: valid`

## Background

The non-executing analysis tool confirmed the fixed MZ/LE headers of `MASTER\PIU_1ST\PIU.EXE`.

The next step is to parse the LE object table and page table without executing original code, then build an initial memory image expected by the protected-mode code.

## Goal

This step implements:

* LE object table parsing
* LE page table parsing
* virtual memory buffers per object
* copying original data pages into object buffers according to the page table
* validation of the entry point object and offset
* object/page/image mapping summaries in the analysis tool output

## Non-Goals

* executing original code
* detailed fixup record parsing
* applying relocations
* selector/descriptor runtime implementation
* Win32 execution transfer

## LE Table Interpretation

The LE object table observed in `PIU.EXE` is interpreted as 24-byte records.

* `virtual_size`
* `relocation_base_address`
* `flags`
* `page_table_index`
* `page_count`
* `reserved`

The LE page table is interpreted as 4-byte records.

* first 3 bytes: big-endian data page number
* last byte: page flags

Data page number 1 points to `data_pages_offset` in the file. N points to `data_pages_offset + (N - 1) * page_size`.

## Output

`repiu_exe_analyzer` adds these values to the existing header output:

* virtual size, flags, and page range per object
* first/last page table record summary
* mapped object count
* total virtual memory size
* total bytes copied from the file
* entry point validation result

## Verification

After the existing build procedure, run:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

Expected output:

* `LE image map: valid`
* `LE mapped objects: 4`
* `LE entry mapping: valid`
