# Task 623 작업 지시서: Linux x64 HLE 경계 쓰기 provenance

## 한국어

### 작업

1. `instruction_emulation.cpp`에서 기존 guest watch 주소를 읽는다.
2. `HandleSegmentPushInstruction`의 watched EIP에 한정하여 HLE 전후 상태를
   기록한다.
3. core probe와 Linux x64 `pumpit2a` 재현을 수행한다.
4. selector/destination/ESP 변화와 fault frontier를
   `docs/analysis/linux-port-frontier.md` 및 작업 로그에 반영한다.

### 제한

* 세그먼트 push의 32-bit guest semantics를 변경하지 않는다.
* guest target, register, memory protection, fault suppression을 변경하지
  않는다.
* watch가 비활성화된 일반 실행의 출력과 동작을 변경하지 않는다.

### 완료 조건

* `core_probe_failures=0`.
* `0x011A643F`의 HLE trace가 실제 selector와 stack destination을 기록한다.
* trace를 켜도 `0x011A6440` fault frontier가 유지된다.

## English

### Work

1. Read the existing guest-watch address in `instruction_emulation.cpp`.
2. Record HLE state before and after the operation only when the watched EIP
   is a segment push.
3. Run the core probe and reproduce Linux x64 `pumpit2a`.
4. Record selector, destination, ESP change, and the fault frontier in
   `docs/analysis/linux-port-frontier.md` and the work log.

### Limits

* Do not change 32-bit guest segment-push semantics.
* Do not change the guest target, registers, memory protection, or fault
  suppression behavior.
* Preserve output and behavior for normal execution with the watch disabled.

### Done criteria

* `core_probe_failures=0`.
* The `0x011A643F` HLE trace records the actual selector and stack destination.
* The `0x011A6440` fault frontier remains unchanged with tracing enabled.
