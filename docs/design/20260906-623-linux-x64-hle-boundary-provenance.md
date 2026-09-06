# Task 623: Linux x64 HLE 경계 쓰기 provenance

## 한국어

### 배경

Task 622는 동적 AOT 대상 `0x011A643A`의 5바이트 명령 뒤에 guest
`PUSH ES`(`0x011A643F`)가 기존 세그먼트 HLE 경계로 남아 있음을 확인했다.
현재 실행은 해당 HLE 뒤 ESP가 `0x0158CC44`로 감소한 다음
`0x011A6440`의 `00 00`에서 `EAX=0x37016BE9` 메모리 접근 fault로 끝난다.
기존 `REPIU_SEGMENT_HLE_TRACE`는 초기 일부 이벤트만 출력하므로 이 특정
경계에서 selector 값과 실제 destination write를 직접 확인할 수 없다.

### 설계

1. 기존 `REPIU_GUEST_WATCH=<guest-address>` 선택을 세그먼트 push HLE에도
   재사용한다.
2. watched guest EIP가 세그먼트 push일 때 opcode, selector, destination,
   value, ESP 전후, EIP 전후를 진단 출력한다.
3. 출력은 기존 guest memory write와 control flow를 수행한 뒤의 관찰 결과이며,
   guest register, memory, HLE semantics, fault handling을 변경하지 않는다.
4. watch 환경 변수가 없으면 추가 parsing이나 출력 없이 기존 경로를 유지한다.

### 검증 전략

* Linux x64 `repiu_core_probe`가 계속 전체 통과하는지 확인한다.
* `REPIU_GUEST_WATCH=0x011A643F`로 `pumpit2a`를 실행한다.
* `PUSH ES` trace가 selector와 destination `0x0158CC44`를 기록하는지,
  이후 frontier가 동일한 `0x011A6440`인지 확인한다.

## English

### Background

Task 622 established that the five-byte dynamic AOT entry at
`0x011A643A` is followed by guest `PUSH ES` (`0x011A643F`) at the existing
segment-HLE boundary. Execution currently lowers ESP to `0x0158CC44` through
that HLE and then faults on the `00 00` bytes at `0x011A6440` while using
`EAX=0x37016BE9` as a memory address. The existing
`REPIU_SEGMENT_HLE_TRACE` prints only an initial subset of events, so it does
not directly expose the selector and destination write at this boundary.

### Design

1. Reuse the existing `REPIU_GUEST_WATCH=<guest-address>` selection for
   segment-push HLE.
2. When the watched guest EIP is a segment push, print its opcode, selector,
   destination, value, ESP before/after, and EIP before/after.
3. Treat the line as observation after the existing guest-memory write and
   control-flow update; do not change guest registers, memory, HLE semantics,
   or fault handling.
4. With the watch variable absent, preserve the existing path without extra
   parsing or output.

### Verification strategy

* Keep the Linux x64 `repiu_core_probe` fully passing.
* Run `pumpit2a` with `REPIU_GUEST_WATCH=0x011A643F`.
* Confirm that the `PUSH ES` trace records the selector and destination
  `0x0158CC44`, and that the subsequent frontier remains
  `0x011A6440`.
