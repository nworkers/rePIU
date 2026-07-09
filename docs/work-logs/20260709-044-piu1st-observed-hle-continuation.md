# piu_1st 관측 기반 HLE 연속 진행 작업 로그

## 시작 상태

현재 정지점은 `0x020FBB24`의 `CD 21`이다. 직전 byte pattern은 `B4 4A CD 21`이고 예외 시점 `EAX=0x00004A2B`이므로 `INT 21h AH=0x4A`로 분류한다.

## 진행 로그

### INT 21h AH=0x4A

trace 기반 `HandleTracedDosInterrupt21`에 `AH=0x4A` 처리를 추가했다. 기존 일반 DOS HLE와 같이 carry flag를 clear하고 `EIP`를 2바이트 진행한다.

`piu_1st`는 `0x020FBB24`의 `INT 21h AH=0x4A`를 통과했고, 새 정지점은 `0x020F8405`의 `C7 04 02 FF FF FF FF`이다.

관측 상태:

* `EAX=0x00000FD0`
* `EDX=0x025D6E84`
* effective write target: `0x025D7E54`
* current relocated placement range: `0x02000000` - `0x025D7000`

이 명령은 `[EDX + EAX]` 위치에 `0xFFFFFFFF`를 쓰는 일반 메모리 write로 해석된다. 대상 주소가 현재 placement 범위를 벗어나므로, 단순 opcode HLE가 아니라 DOS memory block resize 이후 사용할 수 있는 heap/arena 범위를 어떻게 예약하고 commit할지 결정해야 한다.

따라서 이번 연속 처리 작업은 여기서 중단한다. 다음 작업은 `INT 21h AH=0x4A`의 resize 요청을 실제 runtime memory policy와 연결하는 설계가 필요하다.

검증:

* `scripts\test_all.ps1`: 통과
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`: 통과
* OpenWatcom sample pass: 419 / 819
* baseline regression: 0

# piu_1st Observed HLE Continuation Work Log

## Starting State

The current stop is `CD 21` at `0x020FBB24`. The preceding byte pattern is `B4 4A CD 21`, and exception-time `EAX=0x00004A2B`, so it is classified as `INT 21h AH=0x4A`.

## Progress Log

### INT 21h AH=0x4A

Added `AH=0x4A` handling to the trace-based `HandleTracedDosInterrupt21`. Like the existing general DOS HLE path, it clears the carry flag and advances `EIP` by two bytes.

`piu_1st` passed `INT 21h AH=0x4A` at `0x020FBB24`, and the new stop is `C7 04 02 FF FF FF FF` at `0x020F8405`.

Observed state:

* `EAX=0x00000FD0`
* `EDX=0x025D6E84`
* effective write target: `0x025D7E54`
* current relocated placement range: `0x02000000` - `0x025D7000`

This instruction is interpreted as a normal memory write of `0xFFFFFFFF` to `[EDX + EAX]`. Because the target address is outside the current placement range, this is not a simple opcode HLE issue. The project needs a policy for reserving and committing heap/arena space made available after DOS memory block resize.

Therefore, this continuation task stops here. The next task needs a design connecting the `INT 21h AH=0x4A` resize request to the runtime memory policy.

Verification:

* `scripts\test_all.ps1`: passed
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`: passed
* OpenWatcom sample pass: 419 / 819
* baseline regression: 0
