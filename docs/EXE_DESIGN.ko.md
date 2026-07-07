# 실행 파일 설계 노트

## 목적

이 문서는 원본 실행 파일 분석으로 확인한 구조와 로더 설계 결정을 누적 기록한다.

## 현재 확인 사항

`MASTER\PIU_1ST\PIU.EXE`에 대해 현재 확인한 내용:

* 파일은 `MZ` DOS 헤더로 시작한다.
* `MZ` 헤더의 `e_lfanew` 값은 `0x2C90`이다.
* `0x2C90` 위치에는 `LE` 시그니처가 있다.
* 같은 디렉터리에 `DOS4GW.EXE`가 존재하지만, 프로젝트 방향은 DOS4GW를 외부 런타임으로 실행하는 것이 아니라 LE 이미지를 직접 분석하고 필요한 DOS/DPMI 서비스를 HLE로 제공하는 것이다.

## 설계 방향

* 실행 파일 분석은 MZ 파서와 LE 파서로 분리한다.
* 버전별 실행 경로와 자산 루트는 타깃 프로파일로 관리한다.
* 첫 실행 전에는 비실행 분석 도구로 엔트리 포인트, 오브젝트 테이블, 페이지 테이블, fixup 정보를 확인한다.

## 비실행 분석 도구

첫 C++ 도구는 `PIU.EXE`를 실행하지 않고 읽어서 MZ/LE 고정 헤더 정보를 출력한다.

초기 LE 파서는 다음 고정 필드를 해석한다.

* byte order
* word order
* CPU type
* OS type
* module flags
* module page count
* entry object/index
* entry offset
* stack object/index
* stack offset
* page size
* object table offset/count
* object page table offset
* fixup page table offset
* fixup record table offset
* data pages offset

오브젝트 테이블, 페이지 테이블, fixup record의 상세 파싱은 다음 단계에서 누적한다.

## LE 이미지 매핑 Dry-Run

현재 확인한 `PIU.EXE`의 LE 오브젝트 테이블은 24바이트 레코드 4개로 구성된다.

페이지 테이블은 4바이트 레코드이며, 앞 3바이트는 big-endian 형태의 data page 번호, 마지막 1바이트는 page flags로 해석한다.

`data_pages_offset`는 파일 절대 오프셋으로 사용한다. data page 번호 1은 `data_pages_offset`, 번호 N은 `data_pages_offset + (N - 1) * page_size`를 가리킨다.

현재 dry-run 매핑은 각 오브젝트의 `virtual_size`만큼 버퍼를 만들고, 오브젝트가 참조하는 페이지를 해당 버퍼에 복사한다. 마지막 페이지와 오브젝트 끝은 가상 크기를 넘지 않도록 잘라서 복사한다.

이 단계에서는 fixup record를 해석하거나 relocation을 적용하지 않는다.

## LE Fixup Section 분석

LE fixup page table은 `page_count + 1`개의 32-bit little-endian offset으로 해석한다.

각 offset은 fixup record table 시작 기준 상대 offset이다.

page N의 fixup record 범위는 `offset[N]`부터 `offset[N + 1]` 직전까지이다.

현재 단계에서는 이 범위가 단조 증가하는지, fixup record table 크기 안에 들어오는지, page별 fixup span이 얼마나 되는지만 검증한다.

fixup record의 가변 길이 구조와 relocation 적용은 다음 단계에서 별도 설계로 진행한다.

## LE Fixup Record 1차 디코딩

fixup record는 가변 길이이므로, 현재 단계에서는 `PIU.EXE`에서 관찰되는 내부 참조 형태를 우선 지원한다.

공통 record 시작 구조는 `source_type`, `target_flags`, 16-bit `source_offset`으로 해석한다.

내부 target은 1바이트 object 번호와 target offset으로 해석한다. target offset 크기는 `target_flags`의 32-bit offset flag 여부에 따라 16-bit 또는 32-bit로 처리한다.

지원하지 않는 flag 조합은 relocation 실패로 처리하지 않고 unsupported record로 집계한다.

## 내부 Relocation Dry-Run

현재 `PIU.EXE`의 fixup record는 모두 내부 target으로 디코딩된다.

내부 relocation 값은 target object의 `relocation_base_address`와 `target_offset`을 더한 값으로 계산한다.

source 위치는 fixup record의 page index와 source offset을 통해 해당 오브젝트 버퍼 내 offset으로 변환한다.

현재 관찰된 source kind `0x07`은 32-bit little-endian write로 처리한다. 다른 source kind는 selector/포인터 의미가 더 필요하므로 이 단계에서는 skipped로 집계한다.

일부 record는 현재 4KB page buffer 모델에서 직접 write할 수 없는 source offset을 사용하므로 source out-of-range skipped로 집계한다.

## Relocation Skipped Source 분석

skipped relocation은 아직 의미를 확정하지 않고, source kind별 count와 첫 sample을 출력해 다음 단계 설계 근거로 사용한다.

현재 출력해야 하는 sample은 첫 unsupported source kind record와 첫 source out-of-range record이다.

source kind만으로는 상위 source flag 의미를 구분할 수 없으므로 full source type별 count도 함께 출력한다.

unsupported source는 kind별 첫 sample을 출력해 `source_type=0x13`과 순수 `source kind 0x05` 같은 사례를 분리한다.

이 단계는 실행 가능한 메모리에 쓰지 않고, 분석용 LE 이미지 버퍼에만 값을 적용한다.

## DOS/4GW Loader Result

분석 도구와 향후 runtime이 같은 로딩 흐름을 사용하도록 `Dos4gwLoadResult`를 추가한다.

`Dos4gwLoadResult`는 MZ 헤더, LE 헤더, 매핑된 LE 이미지, fixup section 분석 결과, fixup record 디코딩 결과, relocation dry-run 결과를 하나로 묶는다.

`LoadDos4gwExecutable`은 target profile의 format hint가 `DOS4GW_LE`인지 확인한 뒤 기존 MZ/LE/image/fixup/relocation 순서로 결과를 채운다.

## Runtime Memory Dry-Run

`Dos4gwLoadResult`를 입력으로 받아 실행 전 runtime memory 배치 계획을 계산한다.

현재 dry-run은 LE object의 relocation base address와 virtual size를 runtime object region으로 기록한다.

entry linear address는 entry object base와 entry offset을 더해 계산한다.

stack top linear address는 stack object base와 stack offset을 더해 계산한다. stack offset은 object 끝을 가리킬 수 있으므로 object size와 같은 값도 유효하게 본다.

HLE reserve base는 모든 object region 끝의 최댓값을 4KB 단위로 올림 정렬해 계산한다.

## Win32/x86 Runtime Memory Policy

Win32/x86 실행 정책은 runtime memory dry-run 결과를 기반으로 직접 실행 가능성과 필요한 예약 주소 범위를 보고한다.

원본 32-bit x86 entry로 직접 제어를 넘기는 방식은 32-bit host process에서만 지원한다.

64-bit host process에서는 직접 entry 호출을 unsupported로 보고하며, 향후 32-bit helper process 또는 별도 execution backend가 필요하다.

preferred allocation base는 runtime object region 중 가장 낮은 base address이다.

required reserve size는 HLE reserve base에서 preferred allocation base를 뺀 값이다.
