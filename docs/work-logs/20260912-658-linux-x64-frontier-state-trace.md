# 20260912-658 작업 로그: Linux x64 frontier fault 상태 추적

## 한국어

### 결과

Linux x64 fault report와 선택적 execution trace에 `EBP` 출력을 추가했습니다.
이를 통해 `0x010F316C`의 AOT fault 직전 상태와 fault 상태를 같은 guest
context 기준으로 비교할 수 있게 했습니다.

### 확인된 증거

* `0x010F316A` 직전 execution trace: `ESP=0x0158C818`, `EBP=0x5E7BBC68`
* reverse map: cache `0x200022AC` → guest `0x010F316C`, 원본 `89 45 FC`
* fault access: `0x5E7BBC64`, fault `EBP=0x5E7BBC68`
* 정적 map: guest `0x010F3159`의 원본은 `C8 04 00 00`(`ENTER 4,0`)
* 해당 long-mode image slot에는 `ENTER` 대신 `CC`가 배치됨

`EBP`가 fault 직전부터 host stack 주소 형태로 유지되므로,
`MOV [EBP-4],EAX` 자체가 아니라 fallback에서 원본 `ENTER`를 host long mode로
실행한 것이 원인으로 판단됩니다.

### 검증

* Linux x64 Debug `repiu` 및 `repiu_core_probe` 빌드 성공
* `core_probe_total=27`
* `core_probe_failures=0`
* `core_probe_all=true`
* 필터링된 `pumpit2a` 실행에서 위 reverse-map, execution-trace, fault 상태 확인

### 변경하지 않은 범위

원본 guest bytes, AOT `MOV` lowering, return resolver, fault resume 정책은
변경하지 않았습니다. `ENTER` guest 의미론 수정은 659번 후속 작업으로 분리합니다.

## English

### Result

Added `EBP` to the Linux x64 unhandled-fault report and the opt-in execution
trace. This makes it possible to compare the state immediately before the
`0x010F316C` AOT fault with the fault-time guest context.

### Confirmed evidence

* Execution trace immediately before `0x010F316C`: `ESP=0x0158C818`,
  `EBP=0x5E7BBC68`
* Reverse map: cache `0x200022AC` → guest `0x010F316C`, original bytes `89 45 FC`
* Fault access: `0x5E7BBC64`, fault-time `EBP=0x5E7BBC68`
* Static map: guest `0x010F3159` contains `C8 04 00 00` (`ENTER 4,0`)
* The long-mode image slot contains `CC` instead of an emitted `ENTER`

Because the host-stack-shaped `EBP` is already present before the faulting
instruction, the cause is the fallback executing the original `ENTER` in host
long mode, not the `MOV [EBP-4],EAX` lowering itself.

### Verification

* Linux x64 Debug `repiu` and `repiu_core_probe` builds succeeded.
* `core_probe_total=27`
* `core_probe_failures=0`
* `core_probe_all=true`
* The filtered `pumpit2a` run confirmed the reverse-map, execution-trace, and
  fault-state evidence above.

### Unchanged scope

Original guest bytes, AOT `MOV` lowering, the return resolver, and fault-resume
policy were unchanged. Guest `ENTER` semantics are deferred to follow-up Task
659.
