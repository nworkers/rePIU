# 20260726-310 작업 지시: AOT 세그먼트 레지스터 읽기 Fast-Path 구현 / Work order: AOT segment register read fast-path implementation

설계: [20260726-310-aot-segment-register-read-fast-path.md](../design/20260726-310-aot-segment-register-read-fast-path.md)

## 한국어

### 목표

`mov edx, ds` 및 `mov eax, ds` 등 세그먼트 레지스터 읽기(`mov r32, Sreg`) 명령을 AOT Code Cache의 Fast-Path(`AotInstructionKind::kGuardedSegmentRead`)로 구현하여, #DB 예외 트랩을 제거하고 전체 Single-step Handler Latency의 약 **20.42%**를 절감한다.

---

### 작업 내용

1. `include/repiu/runtime/aot_translation_plan.h`
   - `AotInstructionKind::kGuardedSegmentRead` 추가.
   - `AotInstructionRecord` 내 세그먼트 읽기 대상 레지스터 필드 정의.

2. `src/runtime/aot_translation_plan.cpp`
   - `mov r32, Sreg` 디코딩 헬퍼 함수 구현 및 `kGuardedSegmentRead` 분류 추가.
   - `IsHleBoundary`보다 먼저 감지하여 HLE Boundary 분류 방지.

3. `src/runtime/aot_code_cache.cpp`
   - `kGuardedSegmentRead`에 대한 AOT Code Cache 방출기(Emitter) 및 유효성 검증 작성.

4. 검증 및 프로파일링
   - CMake 빌드 및 AOT Probe / Unit Test 실행.
   - 60초 Single-step Hotspot Profile 수행 후 `mov edx, ds` (0x030F940E) 및 `mov eax, ds` (0x030F536A) 트랩 소멸과 전체 Single-step / Handler Ticks 감소 확인.

---

## English

### Objectives

Implement AOT Code Cache Fast-Path (`AotInstructionKind::kGuardedSegmentRead`) for segment register read instructions (`mov r32, Sreg` including `mov edx, ds` and `mov eax, ds`), eliminating #DB exception traps and cutting ~**20.42%** of single-step handler latency.

---

### Tasks

1. `include/repiu/runtime/aot_translation_plan.h`: Add `AotInstructionKind::kGuardedSegmentRead`.
2. `src/runtime/aot_translation_plan.cpp`: Decode `mov r32, Sreg` and classify as `kGuardedSegmentRead` before `IsHleBoundary`.
3. `src/runtime/aot_code_cache.cpp`: Implement Emitter for `kGuardedSegmentRead`.
4. Verification: Build, run tests & AOT probe, and execute 60-second hotspot profile to verify trap elimination and latency reduction.
