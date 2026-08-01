# 20260801-391 Segment-Override 전용 DBT Dispatch / Segment-Override-Specific DBT Dispatch

## 한국어

### 관측

Task 390 캡처에는 AOT segment provenance breakpoint가 21,915건이고 selector guard HLE exit가 21,772건입니다. mismatch와 unresolved site는 모두 0이므로 stale selector 문제가 아니라 low-memory descriptor 정책에 따라 의도적으로 기존 INT3/VEH HLE로 위임된 경로입니다.

남은 effective opcode 상위는 `8A=16,231`, `88=3,859`, `89=3,654`이며 대표 명령은 `mov dl, es:[ebx]`, `mov bl, es:[esi]`, `mov es:[ecx],al`, `mov es:[eax],edi`입니다. 특정 PIU 주소가 아니라 segment-override memory 명령군과 selector policy가 원인입니다.

### 설계

기존 `AotDbtHleDispatchThunk`는 guest register/EFLAGS를 보존해 `DispatchGuestHleInstruction`을 호출하고, 처리 성공 시 cache target으로 복귀하며 처리 불가·상태 불일치는 fail closed합니다. 새 opt-in은 `kSegmentOverrideMem`만 이 slot으로 내보냅니다. 다른 일반 HLE boundary, Port I/O 정책, segment-load/read guard는 변경하지 않습니다.

`REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH=1|on|true`에서만 활성화하고 기본값은 OFF로 둡니다. 이 1차 측정 경로는 native-folded와 low-memory 양쪽 segment override를 같은 HLE dispatcher로 보내므로, 짧은 A/B에서 예외 감소뿐 아니라 전체 비용이 악화되지 않는지 확인한 뒤 hybrid native/HLE slot 또는 기본 승격을 결정합니다.

### 안전 조건

- Zydis가 segment-override memory instruction으로 분류한 기존 `kSegmentOverrideMem`만 대상입니다.
- dispatcher의 VEH-required, unhandled, target-miss, state-mismatch 처리를 그대로 사용합니다.
- coverage validator는 모든 대상 instruction에 완전한 dispatch slot과 fallback fixup이 있는지 확인합니다.
- 비활성화 시 기존 selector-guard native folding 및 INT3/VEH low-memory fallback을 그대로 유지합니다.

## English

### Observation

The Task 390 capture contains 21,915 AOT segment-provenance breakpoints and 21,772 selector-guard HLE exits. Selector mismatch and unresolved-site counts are both zero, so this is an intentional low-memory descriptor-policy delegation to the existing INT3/VEH HLE path rather than stale selector state.

The leading remaining effective opcodes are `8A=16,231`, `88=3,859`, and `89=3,654`. Representative instructions are `mov dl, es:[ebx]`, `mov bl, es:[esi]`, `mov es:[ecx],al`, and `mov es:[eax],edi`. The cause is the segment-override memory class and selector policy, not PIU addresses.

### Design

The existing `AotDbtHleDispatchThunk` preserves guest registers/EFLAGS, calls `DispatchGuestHleInstruction`, and resumes at a cache target on success while failing closed for unsupported or mismatched state. The new opt-in emits only `kSegmentOverrideMem` through that slot. General HLE boundaries, Port I/O policy, and segment load/read guards are unchanged.

Enable only with `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH=1|on|true`; default remains OFF. This first measurement sends both native-foldable and low-memory segment overrides through the HLE dispatcher, so a short A/B must check total cost as well as exception reduction before choosing a hybrid native/HLE slot or default promotion.

### Safety conditions

- Only existing `kSegmentOverrideMem` instructions classified by Zydis are eligible.
- Reuse the dispatcher's VEH-required, unhandled, target-miss, and state-mismatch handling.
- Coverage validation requires a complete dispatch slot and fallback fixup for every eligible instruction.
- Disabled mode retains selector-guard native folding and the INT3/VEH low-memory fallback.
