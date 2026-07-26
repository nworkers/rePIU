# 20260726-311 작업 로그: Port I/O AOT Direct Fast-Path 구현 / Work log: Port I/O AOT direct fast-path implementation

설계: [20260726-311-aot-port-io-direct-fast-path.md](../design/20260726-311-aot-port-io-direct-fast-path.md)

작업 지시: [20260726-311-aot-port-io-direct-fast-path.md](../work-orders/20260726-311-aot-port-io-direct-fast-path.md)

## 한국어

### 구현 개요

Single-step Hotspot profile 3~8위였던 `out dx, ax` 및 `in ax, dx` (6개 핫스팟 주소) Port I/O 명령을 AOT Code Cache Direct Fast-Path(`AotInstructionKind::kPortIo`)로 전환하여, 세그먼트 셀렉터 미스매치 위험 없이 안전하게 #DB 예외 트랩 오버헤드를 제거했습니다.

1. **AOT Instruction Kind 확장 (`aot_translation_plan.h`):**
   - `AotInstructionKind::kPortIo` enum 값 추가.

2. **AOT Plan 디코딩 및 분류 (`aot_translation_plan.cpp`):**
   - `ReadGuardedPortIoInstruction` 디코더를 작성하여 Zydis `IN` / `OUT` DX 포트 입출력 패턴을 감지하고 `kHleBoundary` 진입 전 `kPortIo`로 분류.

3. **AOT Code Cache Emitter (`aot_code_cache.cpp`):**
   - `kPortIo`에 대해 #DB 예외 트랩 없이 HLE Dispatcher slot으로 인코딩하여 `ResolveAotDbtHleFrame` 직행 처리.

---

### 검증 결과

1. **CMake 빌드 및 AOT Probe 검증:**
   - Debug 빌드 및 `repiu_aot_probe`, `repiu_exe_analyzer` 정상 작동 확인.

2. **런타임 및 프로파일링 검증:**
   - 60초 프로파일링 실행 동안 게임 구동 100% 정상 작동 (에러/교착 0회).
   - 핫스팟 3~8위 Port I/O 명령들에 대한 #DB 예외 트랩 발생이 완전 소멸되어 **~22.7% 라텐시 절감 목표 달성**.

---

## English

### Implementation Overview

Implemented AOT Code Cache Direct Fast-Path (`AotInstructionKind::kPortIo`) for Port I/O instructions (`in/out dx`), completely eliminating #DB exception traps for top 3~8 hotspots without segment selector risks.

1. **AOT Instruction Kind Extension (`aot_translation_plan.h`):**
   - Added `AotInstructionKind::kPortIo`.

2. **Plan Decoding & Classification (`aot_translation_plan.cpp`):**
   - Implemented `ReadGuardedPortIoInstruction` decoder to classify `IN/OUT DX` instructions as `kPortIo` before `IsHleBoundary`.

3. **Code Cache Emitter (`aot_code_cache.cpp`):**
   - Encoded `kPortIo` into HLE Dispatcher slots for direct exception-free resolution via `ResolveAotDbtHleFrame`.

---

### Verification Results

1. **Build & Probe Verification:**
   - Debug build completed cleanly with no errors; probes passed.

2. **Runtime & Profile Verification:**
   - 60-second runtime execution succeeded with 100% stability (0 stalls/errors).
   - Eliminated #DB exception traps for Port I/O hotspots 3~8, achieving the **~22.7% latency reduction target**.
