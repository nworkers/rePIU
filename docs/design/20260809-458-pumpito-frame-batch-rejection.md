# 20260809-458 pumpito frame batch 거부 진단 설계 / Pumpito Frame-Batch Rejection Diagnostics Design

## 한국어

### 문제

Task 457 이후 실제 `pumpito` 실행은 `0x030212FD`의 byte fast path에 도달했지만 최종
`batched=0`이었습니다. 정적 `PIU.EXE`의 opcode 서명과 실행 EIP는 예상값과 일치하므로,
fail-closed 계획 생성의 relocation·범위·frame 상태 중 어느 조건이 실제 상태와 다른지
관찰해야 합니다.

### 설계

`piu10_mp3_frame_batch` 내부에 거부 단계별 bit를 두고 guest execution thread의
`ThreadContext`에 이미 보고한 bit mask를 보관합니다. 각 거부 종류는 첫 발생에만 EIP와
판정에 사용한 실제값·예상값을 stderr에 출력합니다. 같은 종류의 이후 실패는 문자열 구성이나
출력 없이 기존 byte 경로로 복귀합니다. 진단은 guest 실행 의미, register, memory, FIFO 또는
decoder 상태를 변경하지 않습니다.

관찰 단계는 capability gate, EIP, code range/signature, relocated immediates, state-address
range, cursor/end/count/target, source range, ring enqueue와 commit입니다. 실제 실패가 확인되면
원본 loop의 명령 순서와 동일한 상태를 보존하는 범위에서 조건 또는 주소 모델을 교정하고,
실제 `PIU.EXE`에서 얻은 사례를 probe에 추가합니다.

### 검증

- Win32 x86 Debug `repiu`와 `repiu_aot_probe`를 build합니다.
- `--piu10` probe가 기존 FIFO와 상태 갱신 계약을 계속 통과하는지 확인합니다.
- `pumpito` 실행에서 최초 거부 사유를 확보하고 원인을 교정합니다.
- 교정 후 `verified frame-tail batch active`와 0보다 큰 `batched`를 확인합니다.

## English

### Problem

After Task 457, a real `pumpito` run reached the byte fast path at `0x030212FD` but finished with
`batched=0`. The static `PIU.EXE` opcode signature and runtime EIP match expectations, so the
specific fail-closed condition among relocation, range, and frame state must be observed.

### Design

Assign one bit to each rejection stage inside `piu10_mp3_frame_batch` and retain the already
reported mask in the guest execution thread's `ThreadContext`. The first occurrence of each kind
prints its EIP and relevant actual and expected values to stderr. Later failures of the same kind
return to the byte path without formatting or output. Diagnostics do not modify guest semantics,
registers, memory, FIFO contents, or decoder state.

Observe the capability gate, EIP, code range/signature, relocated immediates, state-address range,
cursor/end/count/target, source range, ring enqueue, and commit stages. Once the real failure is
known, correct the condition or address model only while preserving the original loop's instruction
order, and add the real `PIU.EXE` case to the probe.

### Verification

- Build Win32 x86 Debug `repiu` and `repiu_aot_probe`.
- Confirm `--piu10` still passes the FIFO and state-update contracts.
- Capture and correct the first rejection reason in a `pumpito` run.
- Confirm `verified frame-tail batch active` and a nonzero `batched` result after correction.
