# Arena 경계 객체 memory store 설계

## 배경

allocator shadow metadata store를 통과한 뒤 `piu_1st`는 relocated base + `0x0001E14C`의 `C7 40 18 00 00 00 00`에서 중단된다.

* `EAX=0x026D6FFC`
* arena end: `0x026D7000`
* destination `EAX+0x18`: `0x026D7014`

객체 base는 실제 arena 내부의 마지막 4바이트에 있지만 초기화 대상 필드는 arena 경계를 넘는다. 이어지는 코드는 같은 base의 `+0x14`, `+0x28` 필드를 `C7 /0`으로 초기화한다.

`INT 21h AH=4Ah` HLE는 selector `0x0024`의 관측 상한을 초과하는 요청에는 insufficient memory를 반환한다. 이번 주소는 이미 allocator가 선택한 arena 내부 base이므로 resize 성공 조건을 다시 조정하기보다 경계에 걸친 객체의 작은 tail을 보존한다.

## 설계

기존 `C7 /0`, `89 /r`, `D9 /2-/3` handler에서 다음 조건을 모두 만족하는 store만 arena-boundary object store로 shadow 처리한다.

* ModR/M이 base register와 displacement를 사용하는 memory destination이다.
* base register 값이 arena 내부이며 arena end 이전 64바이트 안에 있다.
* destination은 arena end 이상이다.
* dword store 전체가 arena end 이후 64바이트 안에 있다.

실제 arena 내부 필드는 계속 네이티브로 실행한다. 64바이트 창 밖의 store, SIB, displacement-only destination은 계속 거부한다.

## 검증

* Win32 x86 빌드
* `piu_1st` 수동 실행에서 연속 `C7` 필드 초기화와 다음 blocker 관측
* 전체 현재 테스트

# Arena-Boundary Object Memory Store Design

## Background

After passing allocator shadow metadata stores, `piu_1st` stops at `C7 40 18 00 00 00 00` at relocated base + `0x0001E14C`.

* `EAX=0x026D6FFC`
* arena end: `0x026D7000`
* destination `EAX+0x18`: `0x026D7014`

The object base occupies the final four bytes inside the real arena, while the initialized field crosses the arena boundary. Following code initializes fields at `+0x14` and `+0x28` from the same base with `C7 /0`.

The `INT 21h AH=4Ah` HLE already returns insufficient memory when requests for selector `0x0024` exceed its observed limit. This base was selected inside the arena by the allocator, so preserve the small boundary-crossing object tail instead of changing resize success semantics again.

## Design

In the existing `C7 /0`, `89 /r`, and `D9 /2-/3` handlers, treat a store as an arena-boundary object store only when all conditions hold:

* ModR/M uses a base register plus displacement memory destination.
* The base-register value is inside the arena and within 64 bytes before arena end.
* The destination is at or above arena end.
* The complete dword store remains within 64 bytes after arena end.

Fields inside the real arena continue to execute natively. Stores outside the 64-byte window, SIB addressing, and displacement-only destinations remain rejected.

## Verification

* Build Win32 x86.
* Run `piu_1st` manually and observe consecutive `C7` field initialization and the next blocker.
* Run the current full test set.
