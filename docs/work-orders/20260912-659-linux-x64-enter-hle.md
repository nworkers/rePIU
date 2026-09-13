# 20260912-659 작업 지시: Linux x64 guest ENTER HLE

설계: [20260912-659 Linux x64 guest ENTER HLE](../design/20260912-659-linux-x64-enter-hle.md)

## 한국어

### 작업 목표

`ENTER` fallback이 host `RSP`/`RBP`를 guest 상태에 유입시키는 Linux x64
frontier를 제거합니다. 원본 게임 로직과 guest bytes는 수정하지 않습니다.

### 작업 항목

1. `HandleEnterInstruction`을 선언하고 구현합니다.
2. `C8 imm16 imm8`의 guest 32비트 frame semantics를 구현합니다.
3. nesting level 0~31 frame chain과 allocation을 처리합니다.
4. source/destination guest range를 사전 검증하고 범위 밖이면 거절합니다.
5. shared HLE dispatch의 opcode `C8` case에서 handler를 호출합니다.
6. `REPIU_ENTER_HLE_TRACE` opt-in 진단 출력으로 실제 guest ESP/EBP 변화를
   확인합니다.
7. Debug build, core probe, bounded `pumpit2a`를 수행합니다.
8. 분석 문서와 작업 로그에 새 frontier와 남은 blocker를 기록합니다.

### 추가된 probe 범위

`general_stack` probe에 non-nested frame, nested display copy, EFLAGS/EIP 보존,
guest range 거절 검사를 추가합니다.

### 변경 금지

* 원본 guest executable bytes와 gameplay logic
* host CPU 전체 에뮬레이션 도입
* `MOV [EBP-4],EAX` lowering의 의미 변경
* fail-closed 범위 거절을 성공 처리로 바꾸는 것

### 최소 검증

* Linux x64 Debug `repiu`/`repiu_core_probe` 빌드 성공
* `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`
* 실제 실행에서 `0x010F3159`의 `ENTER 4,0`이 guest stack을 사용
* `0x010F316C`의 이전 host-stack access fault가 재현되지 않음

## English

### Objective

Remove the Linux x64 frontier where fallback `ENTER` injects host `RSP`/`RBP`
semantics into guest state. Do not modify the original game logic or guest bytes.

### Work items

1. Declare and implement `HandleEnterInstruction`.
2. Implement guest 32-bit semantics for `C8 imm16 imm8`.
3. Handle nesting levels 0–31, frame-chain copying, and allocation.
4. Prevalidate guest source and destination ranges and refuse invalid ranges.
5. Call the handler from the shared HLE dispatcher for opcode `C8`.
6. Add opt-in `REPIU_ENTER_HLE_TRACE` evidence for guest ESP/EBP changes.
7. Run the Debug build, core probe, and bounded `pumpit2a`.
8. Record the new frontier and remaining blocker in the analysis and work log.

### Added probe coverage

The `general_stack` probe now checks a non-nested frame, nested display copying,
EFLAGS/EIP preservation, and guest-range refusal.

### Explicitly unchanged

* Original guest executable bytes and gameplay logic
* No full host CPU emulation
* `MOV [EBP-4],EAX` lowering semantics
* Fail-closed range refusal

### Minimum verification

* Linux x64 Debug `repiu`/`repiu_core_probe` builds succeed.
* `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`.
* The real run shows `ENTER 4,0` using the guest stack.
* The previous host-stack access fault at `0x010F316C` does not recur.

### 결과

`ENTER 4,0`의 guest frame 생성은 확인되었고, 이전 host-stack fault는 제거되었습니다.
실행은 `guest EIP=0x010EFE5F`까지 진행한 뒤 return thunk의 unresolved `INT3`에서
중단되었습니다. 해당 return-dispatch 문제는 659번의 범위 밖인 다음 frontier로
기록합니다.

## Result

Guest frame construction for `ENTER 4,0` is confirmed, and the previous
host-stack fault is cleared. Execution reaches guest `EIP=0x010EFE5F` before
stopping at the return thunk's unresolved `INT3`. That return-dispatch issue is
recorded as the next frontier outside Task 659.
