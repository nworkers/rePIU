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
