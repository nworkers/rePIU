# Task 669 작업 로그: Linux x64 `PUSH r/m32` 공통 lowering

## 한국어

`FF /6`의 32-bit PUSH를 x64 host stack에 의존하지 않고 guest ESP(`R15D`)를
사용하는 stack sequence로 연결했습니다. register, memory, ESP base, absolute
address 형태를 같은 writer에서 처리했습니다.

검증 결과:

- `repiu_core_probe`: `core_probe_total=27`, `core_probe_failures=0`
- `PUSH r/m32` register/memory/ESP/absolute probe 통과
- `GLIDE2X.OVL` DOS attribute success trace 확인
- 실제 실행은 이전 loader frontier를 통과했으나 이후 별도 x64 경계에서 계속 진단함

주소별 예외처리는 추가하지 않았습니다.

## English

Connected 32-bit `FF /6` PUSH to a stack sequence using guest ESP in `R15D`,
without relying on the x64 host stack. Register, memory, ESP-based, and
absolute-address forms share the same writer.

Verification:

- `repiu_core_probe`: `core_probe_total=27`, `core_probe_failures=0`
- Register/memory/ESP/absolute `PUSH r/m32` probes passed
- DOS attribute success for `GLIDE2X.OVL` was observed
- Runtime passed the prior loader frontier and continued into a separate x64
  boundary under investigation

No address-specific exception was added.
