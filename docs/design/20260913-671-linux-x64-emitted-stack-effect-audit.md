# Task 671 설계: Linux x64 emitted stack-effect audit

## 한국어

### 목적

Task 670의 fault trace는 `RET` producer와 target을 확인했지만, host `RSP`가 언제 변경되었는지는 설명하지 못합니다. Linux x64 AOT cache는 원본 guest 명령 하나를 여러 host 명령으로 확장할 수 있으므로, 원본 plan의 분류만 보는 것으로는 최종 emitted 바이트에 raw host stack effect가 남았는지 확인할 수 없습니다.

### 설계 판단

- 기존 `repiu_instruction_census`가 현재 emitter를 사용해 만든 long-mode image를 대상으로 audit를 수행합니다.
- 각 address-map entry의 emitted 바이트를 LONG_64로 다시 decode하고, `PUSH/POP/CALL/RET` 및 RSP를 명시적으로 참조하는 emitted 명령을 bounded하게 보고합니다.
- guest address, plan kind, 원본 바이트, emitted 바이트를 함께 출력해 특정 EIP 예외 없이 emitter 결과와 실행 fault를 연결합니다.
- `PUSHFQ/POPFQ` 같은 의도된 helper stack pair도 관측하되, audit는 의미를 변경하거나 자동으로 허용/거부하지 않습니다.
- audit 자체는 정적 분석 도구에만 추가하며 runtime cache와 guest execution은 변경하지 않습니다.

### 검증 전략

1. Linux x64 `repiu_instruction_census`를 현재 source와 OpenGL 의존성까지 포함해 빌드합니다.
2. `pumpit2a`의 `PIU.EXE`를 대상으로 census를 실행합니다.
3. raw `POP RSP`, raw `RET/CALL`, 또는 stack-pointer operand가 있는 entry를 확인합니다.
4. 결과를 Task 670 work log와 Linux frontier 분석에 반영합니다.

## English

### Purpose

Task 670 identified the `RET` producer and target at the fault, but not when host `RSP` changed. A Linux x64 AOT cache may expand one guest instruction into several host instructions, so inspecting only the original plan does not prove that the final emitted bytes contain no raw host stack effect.

### Design decisions

- Audit the long-mode image built by the existing `repiu_instruction_census`, using the current emitter.
- Decode each address-map entry's emitted bytes again in LONG_64 and boundedly report emitted `PUSH/POP/CALL/RET` instructions and explicit RSP references.
- Print the guest address, plan kind, original bytes, and emitted bytes together so the emitter result can be connected to the runtime fault without an EIP-specific exception.
- Observe intentional helper pairs such as `PUSHFQ/POPFQ` as well; the audit does not automatically allow or reject anything and does not change semantics.
- Keep the audit in the static analysis tool; do not change the runtime cache or guest execution.

### Verification strategy

1. Build the Linux x64 `repiu_instruction_census` with its OpenGL dependency.
2. Run the census on `pumpit2a`'s `PIU.EXE`.
3. Inspect entries containing raw `POP RSP`, raw `RET/CALL`, or stack-pointer operands.
4. Record the result in the Task 670 work log and Linux frontier analysis.
