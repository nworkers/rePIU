# 20260726-311 작업 지시: Port I/O AOT Direct Fast-Path 구현 / Work order: Port I/O AOT direct fast-path implementation

설계: [20260726-311-aot-port-io-direct-fast-path.md](../design/20260726-311-aot-port-io-direct-fast-path.md)

## 한국어

### 목표

Port I/O (`in/out dx`) 명령을 AOT Code Cache Fast-Path (`AotInstructionKind::kPortIo`)로 전환하여, 세그먼트 셀렉터 위험 없이 안전하게 #DB 예외 트랩을 제거하고 전체 Single-step Handler Latency의 약 **22.67%**를 절감한다.

---

### 작업 내용

1. `include/repiu/runtime/aot_translation_plan.h`
   - `AotInstructionKind::kPortIo` 추가.

2. `src/runtime/aot_translation_plan.cpp`
   - `ReadGuardedPortIoInstruction` 디코더 함수 작성.
   - `IN/OUT` DX 패턴 (Opcode `0xEC`, `0xED`, `0xEE`, `0xEF`) 감지 시 `IsHleBoundary` 진입 전 `kPortIo`로 분류.

3. `src/runtime/aot_code_cache.cpp`
   - `kPortIo`용 Emitter 처리 구현.

4. 검증 및 프로파일링
   - CMake 빌드 및 AOT Probe / Unit Test 실행.
   - 런타임 실행 및 프로파일링을 수행하여 핫스팟 3~8위 Port I/O Single-step 트랩 소멸, 100% 정상 게임 구동, 및 ~22.7% 라텐시 절감 검증.

---

## English

### Objectives

Implement AOT Code Cache Fast-Path (`AotInstructionKind::kPortIo`) for Port I/O instructions (`in/out dx`), eliminating #DB exception traps without segment selector risks and cutting ~**22.67%** of single-step handler latency.

---

### Tasks

1. `include/repiu/runtime/aot_translation_plan.h`: Add `AotInstructionKind::kPortIo`.
2. `src/runtime/aot_translation_plan.cpp`: Decode `IN/OUT DX` and classify as `kPortIo` before `IsHleBoundary`.
3. `src/runtime/aot_code_cache.cpp`: Implement Emitter for `kPortIo`.
4. Verification: Build, run tests & AOT probe, verify 100% clean game execution and ~22.7% latency reduction.
