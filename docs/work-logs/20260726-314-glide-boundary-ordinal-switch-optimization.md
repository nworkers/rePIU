# 20260726-314 작업 로그: Glide Boundary Ordinal Switch 최적화 구현 / Work log: Glide Boundary Ordinal Switch Optimization

설계: [20260726-314-glide-boundary-ordinal-switch-optimization.md](../design/20260726-314-glide-boundary-ordinal-switch-optimization.md)

작업 지시: [20260726-314-glide-boundary-ordinal-switch-optimization.md](../work-orders/20260726-314-glide-boundary-ordinal-switch-optimization.md)

## 한국어

### 구현 개요

`linexe_glide_boundary.cpp` 내 Glide HLE Gate 진입 시 매번 발생하던 수십 개의 순차적 문자열 비교(`glide_export->name == "_GR..."`) 체인을 `glide_export->ordinal` (uint16_t) 기반의 **O(1) Direct Jump Table (`switch (glide_export->ordinal)`)** 구문으로 전면 리팩토링했습니다.

1. **Ordinal Direct Switch Dispatcher (`linexe_glide_boundary.cpp`):**
   - 문자열 길이 계산 및 `memcmp` 오버헤드를 완전히 소멸.
   - MSVC/GCC 컴파일러가 조밀한 ordinal integer 바이트를 인덱스로 삼아 O(1) Jump Table을 자동 구성하도록 유도.
   - 각 ordinal 항목별 HLE 스택 파괴(`Esp` 보정) 및 렌더링 백엔드 바인딩 수직 통합.

---

### 검증 결과

1. **CMake 빌드 및 서명 프로브 검증:**
   - Debug 빌드 및 `repiu_glide_issue_probe`, `repiu_aot_probe` 정상 통과.

2. **런타임 동작 검증:**
   - 10초 스모크 테스트 100% 정상 완료 (에러/크래시 0건).
   - 문자열 비교 오버헤드가 제거되어 Glide HLE 관문 지점의 디스패칭 라텐시 및 CPU 부하 최소화 달성.

---

## English

### Implementation Overview

Refactored the linear string comparison chain (`glide_export->name == "_GR..."`) in `linexe_glide_boundary.cpp` into an **O(1) Direct Jump Table (`switch (glide_export->ordinal)`)** based on `glide_export->ordinal` (uint16_t).

1. **Ordinal Direct Switch Dispatcher (`linexe_glide_boundary.cpp`):**
   - Completely eliminated string length calculation and `memcmp` overhead.
   - Enabled compiler generation of an O(1) jump table indexing directly on integer ordinals.
   - Integrated HLE stack adjustment (`Esp` correction) and backend bindings for each ordinal case.

---

### Verification Results

1. **Build & Probe Verification:**
   - Debug build succeeded cleanly; `repiu_glide_issue_probe` and `repiu_aot_probe` passed 100%.

2. **Runtime Verification:**
   - 10-second runtime smoke test passed with zero errors or stalls.
   - Successfully eliminated string comparison overhead during Glide HLE Gate dispatching.
