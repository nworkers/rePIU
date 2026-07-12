# Native return fast path 작업 로그 / Work Log

## 한국어

검증된 guest 함수 본문에서 Trap Flag를 해제하고 guest 반환 주소의 x86 hardware execution breakpoint로 VEH에 복귀하는 fast path를 구현했습니다. relocation operand는 signature 비교에서 제외하고 앞뒤 opcode를 검증합니다. stack·return 범위·signature 검증이 실패하면 활성화하지 않으며, 중간 예외에서는 debug register를 복원하고 기존 single-step HLE로 돌아갑니다.

기능은 `native_fast_path.h/.cpp`로 분리했습니다. Win32 x86 Debug 빌드에 성공했고, 30초 PIU 실행에서 entry/return/cancel은 `9,242/9,242/0`이었습니다. 다음 병목은 인접한 object 2 `+0xDE2xx` helper 집합입니다.

## English

Implemented a fast path that clears Trap Flag inside a verified guest function and reenters VEH through an x86 hardware execution breakpoint at its guest return address. Relocated operands are skipped while surrounding opcodes are verified. Invalid stack, return range, or signature fails closed; an intermediate exception restores debug registers and existing single-step HLE.

The feature is separated into `native_fast_path.h/.cpp`. Win32 x86 Debug builds successfully, and a 30-second PIU run recorded entry/return/cancel counts of `9,242/9,242/0`. The next bottleneck is the adjacent object 2 `+0xDE2xx` helper group.
