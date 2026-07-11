# DS zero-page dword load 설계

## 배경

`piu_1st`는 relocated base + `0x000F7A71`의 `8B 16`에서 중단된다. 명령은 `mov edx, dword ptr [esi]`이며 예외 시점의 `ESI=0`, guest `DS=0x002C`, Win32 host `DS=0x002B`이다. 원본 source는 host 선형 주소 0이 아니라 guest `DS:0`이지만, 일반 `8B /r` handler는 guest selector shadow state를 사용하지 않아 Win32 접근 위반이 발생한다.

실행 이미지의 relocated base를 더하면 guest 저메모리를 실행 이미지 시작으로 잘못 분류한다. 기존 DOS environment buffer 역시 host 환경 문자열로 바로 시작하므로 zero page backing으로 사용할 수 없다.

## 설계

기존 `HandleTracedMemoryLoadInstruction`의 실제 arena 및 shadow-memory read가 실패한 뒤 다음 조건에서만 0을 반환한다.

* guest `DS` selector가 기록되어 있다.
* 명령에 segment override가 없는 `8B /r`이다.
* 계산된 source dword 전체가 첫 4 KiB DOS zero page 안에 있다.

이 범위는 최소 DOS low-memory backing으로 취급한다. 실행 이미지 relocation, 4 KiB 이후 environment 접근, 다른 segment, 임의의 고주소 fault에는 적용하지 않는다.

## 검증

* Win32 x86 Debug 빌드
* `dos4gw_hello` 정상 반환
* `piu_1st`가 `0x000F7A71`을 통과하고 다음 blocker를 관찰
* 전체 테스트

# DS Zero-Page Dword Load Design

## Background

`piu_1st` stops at `8B 16` at relocated base + `0x000F7A71`. The instruction is `mov edx, dword ptr [esi]`; exception state has `ESI=0`, guest `DS=0x002C`, and Win32 host `DS=0x002B`. The original source is guest `DS:0`, not host linear address zero, but the generic `8B /r` handler ignores guest selector shadow state and therefore causes a Win32 access violation.

Adding the relocated image base would incorrectly classify guest low memory as the beginning of the executable image. The existing DOS environment buffer also starts directly with host environment strings, so it cannot serve as zero-page backing.

## Design

After real-arena and shadow-memory reads fail, `HandleTracedMemoryLoadInstruction` returns zero only when a guest `DS` selector is recorded, the instruction is an unprefixed `8B /r`, and the complete source dword lies inside the first 4 KiB DOS zero page. This does not apply to executable relocation, later environment offsets, other segments, or arbitrary high-address faults.

## Verification

* Build Win32 x86 Debug.
* Confirm `dos4gw_hello` returns normally.
* Confirm `piu_1st` passes `0x000F7A71` and exposes the next blocker.
* Run the full test set.
