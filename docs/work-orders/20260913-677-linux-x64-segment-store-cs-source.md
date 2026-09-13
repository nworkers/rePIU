# 20260913-677 Linux x64 segment-store HLE CS source 지원 작업 지시서

## 목표

`8C C8` (`MOV AX, CS`)를 일반적인 segment-store HLE family에 포함하여
Linux x64 runtime이 `0x010F6415`에서 멈추지 않도록 한다. guest CS는 host
CS가 아니라 현재 guest EIP의 selector-table binding으로 유지한다.

## 작업 항목

1. `ReadGuestSegmentSelector`에 logical guest CS 역조회 경로를 추가한다.
2. `HandleSegmentStoreInstruction`에서 `8C /r` source CS를 허용한다.
3. CS source register destination 및 관련 memory destination을 검증한다.
4. 기존 `8E /r` CS load와 unsupported segment encoding refusal을 확인한다.
5. core probe, census, WSL runtime을 실행한다.
6. 분석 문서와 작업 로그에 확인된 frontier 및 남은 문제를 기록한다.

## 제한

* 특정 guest EIP만 검사하는 분기나 예외처리를 추가하지 않는다.
* 실제 host segment register를 guest selector state로 사용하지 않는다.
* 원본 guest instruction bytes나 gameplay logic을 수정하지 않는다.

## 완료 기준

1. `8C C8`가 CS source selector를 current guest EIP에 맞게 기록한다.
2. selector table 역조회 실패 시 안전하게 거부한다.
3. 기존 core probe가 모두 통과한다.
4. runtime이 `0x010F6415` 이후의 새로운 frontier까지 진행한다.

## English

### Objective

Include `8C C8` (`MOV AX, CS`) in the generic segment-store HLE family so the
Linux x64 runtime does not stop at `0x010F6415`. Guest CS must come from the
selector-table binding for the current guest EIP rather than host CS.

### Tasks

1. Add logical guest-CS reverse lookup to `ReadGuestSegmentSelector`.
2. Allow CS as the source of `8C /r` in `HandleSegmentStoreInstruction`.
3. Verify CS register and related memory destination forms.
4. Confirm the existing refusal of `8E /r` CS loads and unsupported encodings.
5. Run the core probe, census, and WSL runtime.
6. Record the confirmed frontier and remaining issue in the analysis and work log.

### Limits

* Do not add a branch for a single guest EIP.
* Do not use a real host segment register as guest selector state.
* Do not modify original guest bytes or gameplay logic.

### Done criteria

1. `8C C8` records the CS selector for the current guest EIP.
2. Missing selector-table resolution fails closed.
3. The existing core probe passes.
4. The runtime reaches a new frontier after `0x010F6415`.
