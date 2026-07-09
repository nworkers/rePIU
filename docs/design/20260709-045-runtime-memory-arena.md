# Runtime Memory Arena 설계

## 배경

현재 `piu_1st`는 `INT 21h AH=0x4A`를 최소 성공 응답으로 통과한 뒤 `0x020F8405`의 `C7 04 02 FF FF FF FF`에서 중단된다. 이 명령은 `[EDX + EAX]`에 `0xFFFFFFFF`를 쓰려는 일반 메모리 write이며, 관측된 대상 주소 `0x025D7E54`는 현재 relocated placement 끝 `0x025D7000`을 넘어선다.

이 문제는 opcode 자체가 특권 명령이어서가 아니라, DOS/4G 런타임이 resize 이후 사용할 수 있다고 믿는 arena/heap 범위를 host 쪽에서 아직 보장하지 않기 때문에 발생한다.

## 목표

완전한 DOS 메모리 관리자를 만들지 않는다. 이번 단계에서는 원본 런타임이 요구하기 시작한 확장 가능 runtime arena를 명시적인 구조로 관리한다.

## 구조

공용 runtime 계층에 `RuntimeMemoryArenaPlan`을 추가한다.

* `base_address`: relocated runtime arena 시작 주소
* `image_reserve_size`: 기존 이미지/프로파일이 요구한 최소 reserve 크기
* `expansion_slack_size`: DOS/4G HLE가 관측 기반으로 추가 확보하는 여유 크기
* `arena_reserve_size`: 실제 Win32 placement와 probe에 사용할 reserve 크기
* `arena_end_address`: arena 끝 주소

초기 정책은 `image_reserve_size + 0x00010000`을 4KB 정렬한 값을 `arena_reserve_size`로 사용한다. 이 64KB slack은 현재 blocker의 대상 주소 `0x025D7E54`를 포함하기 위한 최소 관측 기반 확장이다.

## Win32 연결

`SelectRelocatedImageBase`와 `PlaceWin32RelocatedImage`는 기존 target reserve hint 대신 arena plan의 `arena_reserve_size`를 사용한다. 현재 Win32 placement는 reserve 범위 전체를 commit하므로, arena 범위 안의 후속 write는 실제 host memory로 접근 가능해야 한다.

## 향후 확장

다음 단계에서는 `INT 21h AH=0x4A` 입력값을 `RuntimeMemoryArenaResizeRequest` 형태로 기록하고, resize 성공/실패를 arena plan의 committed/available 상태와 연결한다.

# Runtime Memory Arena Design

## Background

`piu_1st` currently passes the minimal success response for `INT 21h AH=0x4A`, then stops at `C7 04 02 FF FF FF FF` at `0x020F8405`. The instruction is a normal memory write of `0xFFFFFFFF` to `[EDX + EAX]`, and the observed target address `0x025D7E54` is beyond the current relocated placement end `0x025D7000`.

This happens not because the opcode itself is privileged, but because the host side does not yet guarantee the arena/heap range that the DOS/4G runtime believes is available after resize.

## Goal

Do not build a complete DOS memory manager. This step introduces an explicit structure for the expandable runtime arena that the original runtime has started to require.

## Structure

Add `RuntimeMemoryArenaPlan` to the shared runtime layer.

* `base_address`: start of the relocated runtime arena
* `image_reserve_size`: minimum reserve size required by the current image/profile
* `expansion_slack_size`: observed extra space reserved for DOS/4G HLE
* `arena_reserve_size`: actual reserve size used by Win32 placement and probing
* `arena_end_address`: end address of the arena

The initial policy uses `image_reserve_size + 0x00010000`, aligned to 4KB, as `arena_reserve_size`. This 64KB slack is the minimal observed expansion needed to include the current blocker target `0x025D7E54`.

## Win32 Integration

`SelectRelocatedImageBase` and `PlaceWin32RelocatedImage` use `arena_reserve_size` instead of the previous target reserve hint. The current Win32 placement commits the full reserved range, so later writes inside the arena should be backed by real host memory.

## Future Extension

The next step should record `INT 21h AH=0x4A` inputs as a `RuntimeMemoryArenaResizeRequest` and connect resize success/failure to the arena's committed/available state.
