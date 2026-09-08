# Task 640 작업 로그: 특정 주소를 포함하는 동적 AOT plan 추적

설계: [20260908-640](../design/20260908-640-dynamic-aot-containing-plan-trace.md)  
작업 지시: [20260908-640](../work-orders/20260908-640-dynamic-aot-containing-plan-trace.md)

## 한국어

`REPIU_AOT_DYNAMIC_CONTAINS`를 추가했습니다. 동적 image의 address map이 지정 guest
주소를 포함할 때 요청 entry의 raw bytes, 요청 및 contains instruction의 block 위치와
metadata, 관련 fixup을 출력합니다. 설정하지 않으면 기존 출력과 실행은 바뀌지 않습니다.

실제 `pumpit2a` 캡처에서 contains `0x011C8E0E`는 요청 entry `0x011A8E10`에서 시작한
단일 block의 65,536번째이자 마지막 `00 00` instruction이었습니다. block은 정확히
`0x20000` guest bytes의 zero-filled 영역이며 마지막 미해결 fallthrough target은
`0x011C8E10`입니다. 따라서 Task 639는 잘못된 target을 만든 것이 아니라 기존 zero-block
실행을 정확히 이어 준 것입니다.

### 검증

* Linux x64 `repiu` 빌드: 통과
* 실제 contains trace: request `0x011A8E10`, block index `0..65535`, tail fixup 확인
* Linux x64 core probe: 통과 (`24/24`)

## English

Added `REPIU_AOT_DYNAMIC_CONTAINS`. When a dynamic image's address map contains
the selected guest address, it reports the request entry's raw bytes, block and
instruction metadata for both request and contained addresses, and related
fixups. With the setting absent, output and execution remain unchanged.

In the real `pumpit2a` capture, contained address `0x011C8E0E` is the 65,536th
and final `00 00` instruction of one block requested at `0x011A8E10`. The block
covers exactly `0x20000` zero-filled guest bytes and its unresolved fallthrough
targets `0x011C8E10`. Task 639 therefore did not invent the bad target; it
correctly continued pre-existing zero-block execution.

### Verification

* Linux x64 `repiu` build: passed
* Real contains trace: request `0x011A8E10`, block indices `0..65535`, tail fixup confirmed
* Linux x64 core probe: passed (`24/24`)
