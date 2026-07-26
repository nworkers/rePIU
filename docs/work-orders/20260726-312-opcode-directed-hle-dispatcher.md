# 20260726-312 작업 지시: Opcode-directed HLE Dispatcher 구현 / Work order: Opcode-directed HLE dispatcher implementation

설계: [20260726-312-opcode-directed-hle-dispatcher.md](../design/20260726-312-opcode-directed-hle-dispatcher.md)

## 한국어

### 목표

Opcode-directed Direct Lookup Table (Jump Table)을 구축하여 HLE 트랩 발생 시 순차 검사 오버헤드를 제거하고, AOT Native Code Cache 체류율(Residency)을 극대화한다.

---

### 작업 내용

1. `src/hle/hle_dispatcher.cpp`
   - 256-entry `OpcodeHleHandlerTable` 구조 구축.
   - `DispatchGuestHleInstruction`에 Opcode-directed direct dispatch 룩업 추가.

2. `src/platform/win32/aot/aot_dbt_hle_dispatch.cpp`
   - AOT Re-entry 시 target EIP 룩업 및 Direct Edge Linking 최적화.

3. 검증 및 프로파일링
   - CMake 빌드 및 AOT Probe / Unit Test 실행.
   - 런타임 프로파일링 실행으로 HLE 핸들러 평균 Latency 감소 및 게임 구동 100% 정상 작동 확인.

---

## English

### Objectives

Build an Opcode-directed Direct Lookup Table (Jump Table) to eliminate linear scan overhead during HLE traps and maximize AOT Native Code Cache residency.

---

### Tasks

1. `src/hle/hle_dispatcher.cpp`: Implement 256-entry `OpcodeHleHandlerTable` for O(1) direct opcode dispatch.
2. `src/platform/win32/aot/aot_dbt_hle_dispatch.cpp`: Optimize target EIP resolution during AOT re-entry.
3. Verification: Build, run tests & AOT probe, verify latency reduction and clean execution.
