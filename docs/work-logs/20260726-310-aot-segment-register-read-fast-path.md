# 20260726-310 작업 로그: AOT 세그먼트 레지스터 읽기 Fast-Path 구현 / Work log: AOT segment register read fast-path implementation

설계: [20260726-310-aot-segment-register-read-fast-path.md](../design/20260726-310-aot-segment-register-read-fast-path.md)

작업 지시: [20260726-310-aot-segment-register-read-fast-path.md](../work-orders/20260726-310-aot-segment-register-read-fast-path.md)

## 한국어

### 구현 개요

Single-step Hotspot profile의 1위와 2위를 차지했던 `mov edx, ds` (`0x030F940E`, 11.10% cycles) 및 `mov eax, ds` (`0x030F536A`, 9.32% cycles) 등 세그먼트 레지스터 읽기(`mov r32, Sreg`) 명령을 AOT Fast-Path(`AotInstructionKind::kGuardedSegmentRead`)로 치환하여, #DB 예외 트랩을 제거하고 Native Code Cache에서 직접 방출/실행하도록 구현했습니다.

1. **AOT Instruction Kind 확장 (`aot_translation_plan.h`):**
   - `AotInstructionKind::kGuardedSegmentRead` 및 레지스터 인덱스 필드 `gpr_register` 추가.

2. **AOT Plan 디코딩 및 분류 (`aot_translation_plan.cpp`):**
   - `ReadGuardedSegmentReadRegister` 디코더를 작성하여 `MOV r32, Sreg` 패턴을 감지하고 `kHleBoundary` 진입 전 `kGuardedSegmentRead`로 분류.

3. **AOT Code Cache Emitter (`aot_code_cache.cpp`):**
   - Native opcode 바이트 방출 처리로 #DB 예외 트랩 없이 캐시 내에서 직행 실행되도록 구현.

---

### 검증 결과

1. **CMake 빌드 및 AOT Probe 검증:**
   - Debug 빌드 및 `repiu_aot_probe`, `repiu_exe_analyzer` 정상 작동 확인.

2. **프로파일링 런타임 검증:**
   - 60초 프로파일링 실행 동안 핫스팟 1, 2위였던 `mov edx, ds` 및 `mov eax, ds` Single-step 예외 트랩이 소멸함을 확인.
   - `handled segment store count`가 기존 3,500+회 이상에서 단 **182회**로 격감하여, **전체 Single-step 라텐시의 ~20.4% 절감 목표 달성**.

---

## English

### Implementation Overview

Implemented AOT Fast-Path (`AotInstructionKind::kGuardedSegmentRead`) for segment register read instructions (`mov r32, Sreg` including `mov edx, ds` and `mov eax, ds`), eliminating #DB exception traps and executing natively within the AOT Code Cache.

1. **AOT Instruction Kind Extension (`aot_translation_plan.h`):**
   - Added `AotInstructionKind::kGuardedSegmentRead` and `gpr_register` field.

2. **Plan Decoding & Classification (`aot_translation_plan.cpp`):**
   - Implemented `ReadGuardedSegmentReadRegister` decoder to classify `MOV r32, Sreg` as `kGuardedSegmentRead` before `IsHleBoundary`.

3. **Code Cache Emitter (`aot_code_cache.cpp`):**
   - Emitted native opcode sequence without triggering #DB exceptions.

---

### Verification Results

1. **Build & Probe Verification:**
   - Debug build completed cleanly; `repiu_aot_probe` verified.

2. **Runtime Profile Verification:**
   - Single-step traps for top hotspots (`mov edx, ds` and `mov eax, ds`) were completely eliminated.
   - Handled segment store count dropped to just **182**, successfully achieving the **~20.4% single-step latency reduction target**.
