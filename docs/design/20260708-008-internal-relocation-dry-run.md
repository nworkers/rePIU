# 내부 Relocation Dry-Run 설계

## 배경

이전 단계에서 `PIU.EXE`의 fixup record table 전체를 1차 디코더로 소비했고, 모든 record가 내부 target으로 해석됨을 확인했다.

다음 단계는 원본 코드를 실행하지 않고, 매핑된 LE 이미지 버퍼에 내부 relocation 값을 적용해 보는 것이다.

## 목표

이번 단계는 다음을 구현한다.

* fixup source page가 속한 오브젝트 찾기
* source offset을 오브젝트 버퍼 내 write 위치로 변환
* target object와 target offset을 가상 주소 값으로 변환
* source kind `0x07` record에 32-bit little-endian relocation 값 쓰기
* 아직 해석하지 않는 source kind는 skipped로 집계
* 현재 page/object 버퍼 범위에 직접 쓸 수 없는 source offset은 source out-of-range skipped로 집계
* 적용된 relocation 수, skipped 수, 실패 수, 첫 적용 정보 출력

## 비목표

* 원본 코드 실행
* Win32 실행 메모리 할당
* selector/descriptor 구현
* external/import relocation 처리
* source kind `0x07` 외 relocation write 형식 적용

## 주소 계산

내부 target relocation 값은 다음으로 계산한다.

```text
target_virtual_address = target_object.relocation_base_address + target_offset
```

source write 위치는 record의 `page_index`와 `source_offset`을 사용해 찾는다.

```text
object_page_index = page_index - (object.page_table_index - 1)
object_offset = object_page_index * page_size + source_offset
```

현재 `PIU.EXE`에는 source kind `0x07`과 다른 source kind가 함께 존재한다. 이 단계에서는 source kind `0x07`만 32-bit offset write로 적용하고, 나머지는 skipped로 기록한다.

일부 record는 `source_offset=0xFFFE`처럼 현재 4KB page buffer 기준으로 직접 write할 수 없는 위치를 가리킨다. 이 단계에서는 이 record를 실패로 처리하지 않고 source out-of-range skipped로 집계한다.

## 검증 기준

분석 도구 출력에서 다음을 확인한다.

* `LE relocation dry run: valid`
* applied relocation count와 skipped relocation count의 합이 decoded fixup record count와 같다.
* failed relocation count가 0이다.

## 다음 단계

다음 단계는 relocation이 적용된 이미지를 기반으로 RuntimeMemory와 entry/stack dry-run을 설계한다.

## Background

The previous step consumed the entire `PIU.EXE` fixup record table with the first-pass decoder and confirmed that all records are internal targets.

The next step applies internal relocation values to the mapped LE image buffers without executing original code.

## Goal

This step implements:

* finding the object that owns each fixup source page
* converting source offset into a write location inside an object buffer
* converting target object and target offset into a virtual address value
* writing a 32-bit little-endian relocation value for source kind `0x07`
* counting source kinds not interpreted yet as skipped
* counting source offsets that cannot be directly written into the current page/object buffers as source out-of-range skipped
* printing applied relocation count, skipped count, failure count, and first applied relocation

## Non-Goals

* executing original code
* allocating Win32 executable memory
* selector/descriptor implementation
* external/import relocation handling
* applying relocation write formats other than source kind `0x07`

## Address Calculation

Internal target relocation values are calculated as:

```text
target_virtual_address = target_object.relocation_base_address + target_offset
```

The source write location is found from the record's `page_index` and `source_offset`.

```text
object_page_index = page_index - (object.page_table_index - 1)
object_offset = object_page_index * page_size + source_offset
```

`PIU.EXE` contains source kind `0x07` and other source kinds. This step applies only source kind `0x07` as a 32-bit offset write and records the rest as skipped.

Some records point to locations such as `source_offset=0xFFFE`, which cannot be written directly into the current 4 KB page buffer model. This step records those records as source out-of-range skipped instead of failures.

## Verification Criteria

The analysis tool output must confirm:

* `LE relocation dry run: valid`
* applied relocation count plus skipped relocation count equals decoded fixup record count
* failed relocation count is 0

## Next Step

The next step designs RuntimeMemory and entry/stack dry-run based on the relocated image.
