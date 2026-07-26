# 20260726-318 작업 지시: Glide Ordinal 심볼릭 상수 도입 구현 / Work order: Glide Ordinal Symbolic Constants

설계: [20260726-318-glide-ordinal-symbolic-constants.md](../design/20260726-318-glide-ordinal-symbolic-constants.md)

## 한국어

### 목표

`include/repiu/hle/glide_hle.h`에 `repiu::hle::glide_ordinal` 심볼릭 상수를 정의하고, `linexe_glide_boundary.cpp` 내의 하드코딩된 ordinal 숫자 리터럴을 100% 심볼릭 상수로 교체하여 코드 가독성과 유지보수성을 극대화한다.

---

### 작업 내용

1. `include/repiu/hle/glide_hle.h`
   - `namespace repiu::hle::glide_ordinal` 생성.
   - 모든 50여 개 Glide API의 ordinal 수치를 `constexpr std::uint16_t` 심볼릭 상수로 정의 (`kGrGlideInit = 1U`, `kGrSstWinOpen = 9U`, `kGrDrawTriangle = 52U` 등).

2. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `switch (glide_export->ordinal)` 내부의 모든 `case 1:`, `case 9:`, `case 52:` 리터럴을 `case go::kGrGlideInit:`, `case go::kGrSstWinOpen:`, `case go::kGrDrawTriangle:` 등 심볼릭 상수로 전면 교체.
   - Telemetry 및 진단 모드 로깅 조건식의 리터럴 수치도 심볼릭 상수로 전면 교체.

3. 검증
   - Debug 빌드 (`cmake --build build --config Debug`).
   - 10초 스모크 런타임 테스트 실행.

---

## English

### Objectives

Introduce `repiu::hle::glide_ordinal` symbolic constants in `include/repiu/hle/glide_hle.h` and replace 100% of hardcoded ordinal numeric literals in `linexe_glide_boundary.cpp` with symbolic constants.

---

### Tasks

1. Define `repiu::hle::glide_ordinal` namespace with `constexpr std::uint16_t` constants in `glide_hle.h`.
2. Replace all numeric `case` labels and conditional literals in `linexe_glide_boundary.cpp` with symbolic constants.
3. Build Debug config and run 10s smoke runtime test.
