# 20260726-314 작업 지시: Glide Boundary Ordinal Switch 최적화 구현 / Work order: Glide Boundary Ordinal Switch Optimization

설계: [20260726-314-glide-boundary-ordinal-switch-optimization.md](../design/20260726-314-glide-boundary-ordinal-switch-optimization.md)

## 한국어

### 목표

`linexe_glide_boundary.cpp` 내 Glide HLE Gate의 순차 문자열 비교문을 `switch (glide_export->ordinal)` 기반 O(1) Direct Jump Table로 전면 리팩토링하여 디스패칭 오버헤드를 완전히 제거한다.

---

### 작업 내용

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `glide_export->name == "_GR..."` 문자열 비교 체인을 `switch (glide_export->ordinal)` 구문으로 대체.
   - 각 ordinal case에 해당하는 HLE 핸들러 및 스택 정돈 수직 통합.

2. 검증 및 프로파일링
   - CMake Debug 빌드 및 `repiu_glide_issue_probe` / `repiu_aot_probe` 검증.
   - 런타임 10초 스모크 테스트 실행으로 정상 동작 확인.

---

## English

### Objectives

Refactor the string comparison chain in `linexe_glide_boundary.cpp` to an O(1) Direct Jump Table (`switch (glide_export->ordinal)`), completely eliminating string comparison overhead in the Glide HLE Gate.

---

### Tasks

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`: Replace string equality checks with `switch (glide_export->ordinal)` cases.
2. Verification: Build cleanly, run probes, and verify smooth 10-second runtime execution.
