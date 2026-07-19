# Host exception address provenance design

## 목적 / Purpose

guest `0x0304ED35` 이후 반복되는 host `0x1017E76A` access violation의 실제 실행 메모리
영역과 opcode를 예외 시점에 기록합니다.

Record the executing memory region and opcodes at exception time for the recurring host
`0x1017E76A` access violation after guest `0x0304ED35`.

## 정책 / Policy

Win32 exception capture는 exception address에 `VirtualQuery`를 수행하고, 할당 base,
region size, protection, allocation protection 및 최대 16 byte를 stderr diagnostic으로
기록합니다. 상태 변경이나 예외 복구 정책은 바꾸지 않습니다.

Win32 exception capture queries the exception address with `VirtualQuery` and writes its
allocation base, region size, protection, allocation protection, and up to 16 bytes to
stderr diagnostics. It changes neither state nor recovery policy.
