# DOS Resize Guard 작업 지시

## 목표

`piu_1st`의 반복적인 `INT 21h AH=0x4A` resize 흐름이 arena 밖 metadata write로 이어지지 않도록 관측 기반 상한을 적용한다.

## 작업 범위

* execution attempt 구조에 DOS resize 관측 필드를 추가한다.
* traced DOS interrupt의 `AH=0x4A` 처리에서 `guest_es`, `BX`, 결과를 기록한다.
* `guest_es == 0x0024`이고 `BX > 0xE700`인 요청은 DOS insufficient memory 오류로 실패시킨다.
* loader 로그에 마지막 resize 요청 정보를 출력한다.
* `piu_1st` 실행으로 resize 입력값과 다음 blocker를 확인한다.

## 제외 범위

* DOS MCB 전체 semantics는 구현하지 않는다.

# DOS Resize Guard Work Order

## Goal

Apply an observed guard so the repeated `INT 21h AH=0x4A` resize flow in `piu_1st` does not lead to an out-of-arena metadata write.

## Scope

* Add DOS resize observation fields to the execution attempt structure.
* Record `guest_es`, `BX`, and the result in traced DOS interrupt `AH=0x4A` handling.
* Fail requests with DOS insufficient memory when `guest_es == 0x0024` and `BX > 0xE700`.
* Print the last resize request in loader logs.
* Run `piu_1st` to inspect resize inputs and the next blocker.

## Out of Scope

* Do not implement full DOS MCB semantics.
