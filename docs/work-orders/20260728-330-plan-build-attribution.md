# 20260728-330 작업 지시: plan build 귀속과 Debug 왜곡 분리 / Work order

설계: [docs/design/20260728-330-plan-build-attribution.md](../design/20260728-330-plan-build-attribution.md)

## 한국어

### 목표

append의 39.94%가 된 `plan_build`(명령당 약 `25,433 tick`)를 6단계로 귀속하고(Part A),
같은 코드·같은 입력을 Debug와 Release로 측정해 **Debug 왜곡 계수**를 분리합니다
(Part B). 관측 전용이며 최적화는 하지 않습니다.

### 구현 항목

1. `include/repiu/runtime/cycle_clock.h` (신규)
   - 공용 `ReadCycleCounter()`. x86 MSVC는 `__rdtsc`, x86 GCC/Clang은
     `__builtin_ia32_rdtsc`, 그 외는 `steady_clock`.
   - 감소 표본을 0으로 clamp하는 `CycleDelta(start, end, clamped_count*)`.
2. `include/repiu/runtime/aot_translation_plan.h`
   - `AotPlanBuildProfile` POD 추가: 단계 6개 cycle, `total_cycles`,
     `decode_count`, `record_count`, `sweep_pass_count`,
     `sweep_record_visit_count`, `clamped_sample_count`, `enabled`.
   - `excluded_ranges` 오버로드에 후행 기본 인자
     `AotPlanBuildProfile* profile = nullptr` 추가.
3. `src/runtime/aot_translation_plan.cpp`
   - 설계 3절의 6단계를 계측합니다. **분기 순서와 부수효과 순서를 바꾸지 않습니다.**
   - `profile == nullptr`이면 timestamp를 읽지 않습니다.
4. `include/repiu/platform/win32/aot_worker_timing.h`,
   `src/platform/win32/aot/aot_worker_timing.cpp`
   - plan 단계 누적 필드와 `RecordAotPlanBuildProfile` 추가. 공용 POD를 그대로 받아
     누적만 합니다.
5. `src/platform/win32/aot_code_cache_win32.cpp`
   - `timing != nullptr`일 때만 지역 `AotPlanBuildProfile`을 만들어 전달하고 누적합니다.
6. `src/host/win32/main.cpp`
   - 단계 tick/비율/residual과 규모(회당 decode·record·sweep pass)를 출력합니다.
7. `src/tools/aot_probe/plan_build_benchmark_probe.{h,cpp}` (신규), `CMakeLists.txt`,
   `src/tools/aot_probe/main.cpp`
   - 실제 이미지·entry로 plan build를 반복 측정해 명령당 tick과 단계 분포를 출력합니다.
   - 판정은 결정적 조건만 사용합니다(plan 유효, 단계 합 <= 총합, 카운터 정합).
     **시간 값 자체는 통과 조건으로 쓰지 않습니다.**

### 안전 조건

- 관측 전용. plan 결과는 계측 ON/OFF에서 바이트 단위로 동일해야 합니다.
- 공용 파일(`src/runtime/`)에 Win32 헤더를 넣지 않습니다.
- 기존 호출 지점의 인자 순서를 바꾸지 않습니다(후행 기본 인자만 추가).
- 워커 스레드 전용 경로이므로 원자 연산·잠금을 추가하지 않습니다.
- `repiu_aot_probe`의 기존 그룹이 계속 통과해야 합니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1` (Debug)
2. `cmake --build build/win32_x86_debug --config Release --target repiu_aot_probe`
3. 두 구성에서 `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 통과.
   신규 `plan_build_bench_*` 포함.
4. Debug/Release의 명령당 tick 비율로 설계 5절 gate B를 판정합니다.
5. `arena_view` probe로 plan 동등성이 유지되는지 재확인합니다(계측이 결과를 바꾸지
   않음).
6. 실게임 60초 in-situ 분포는 사용자 확인 후 별도로 수행합니다.

---

## English

### Goal

Attribute `plan_build`, now 39.94% of an append at about `25,433` ticks per instruction, across
six stages (Part A), and separate the Debug distortion factor by measuring the same code and
input in Debug and Release (Part B). Observation only; nothing is optimized.

### Implementation

Add a platform-neutral `cycle_clock.h` with `ReadCycleCounter()` and a clamping `CycleDelta`, an
`AotPlanBuildProfile` POD in the plan header, and a trailing defaulted profile argument on the
excluded-ranges overload. Instrument the six stages in the builder without changing branch or
side-effect order, reading no timestamps when the profile is null. Accumulate the neutral POD in
the Win32 worker timing profile, pass a local profile from the append only when timing is
enabled, report stages and scale in the loader summary, and add a probe benchmark that builds a
plan repeatedly from the real image and reports ticks per instruction plus the stage
distribution, judged only by deterministic conditions rather than by timing values.

### Safety

Observation only, with plans byte-for-byte identical whether instrumentation is on or off. No
Win32 header enters `src/runtime/`. Existing call sites keep their argument order because the new
parameter is trailing and defaulted. No atomics or locks are added, and the existing probe groups
must keep passing.

### Verification

Build Debug, build the probe in Release, pass the whole probe suite in both configurations
including the new `plan_build_bench_*` group, judge design gate B from the Debug-to-Release ratio
of ticks per instruction, and re-confirm plan equivalence through the `arena_view` probe. The
60-second in-situ distribution is run separately after user confirmation.
