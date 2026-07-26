# 20260726-316 작업 로그: Glide Boundary 전체 Ordinal Switch 단일 통합 / Work log: Unified Glide Boundary Ordinal Switch

작업 지시: [20260726-316-unified-glide-boundary-ordinal-switch.md](../work-orders/20260726-316-unified-glide-boundary-ordinal-switch.md)

## 한국어

### 작업 요약

`src/platform/win32/boundary/linexe_glide_boundary.cpp` 내의 모든 `glide_export->name == "_GR..."` 문자열 비교 구문을 전면 제거하고, 50여 개 전체 Glide API를 단일 `switch (glide_export->ordinal)` O(1) Direct Jump Table 구문으로 수직 통합하였습니다.

---

### 변경 세부 사항

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `_GRGLIDEINIT@0` (1), `_GRBUFFERCLEAR@12` (2), `_GRBUFFERSWAP@4` (3), `_GRSSTQUERYHARDWARE@4` (6), `_GRSSTSELECT@4` (7), `_GRSSTWINOPEN@28` (9), `_GRCOLORMASK@8` (32), `_GRRENDERBUFFER@4` (33), `_GRDEPTHMASK@4` (34), `_GRLFBLOCK@24` (47), `_GRLFBUNLOCK@8` (48), `_GRDRAWTRIANGLE@12` (52), `_GRCULLMODE@4` (99), `_GRDITHERMODE@4` (100) 등 50여 개 Glide API 전체를 단일 `switch (glide_export->ordinal)` Jump Table로 수직 통합.
   - 코드 베이스 내 `glide_export->name` 비교 0개 달성.

2. 검증
   - Debug 빌드 완료 (`cmake --build build --config Debug`).
   - 10초 스모크 런타임 테스트 통과 (Access Violation 및 0xC0000005 예외 없음).

---

## English

### Task Summary

Refactored all 50+ Glide API handlers in `src/platform/win32/boundary/linexe_glide_boundary.cpp`, completely eliminating `glide_export->name` string comparisons in favor of a single unified `switch (glide_export->ordinal)` O(1) Direct Jump Table.

---

### Key Changes

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - Consolidated 100% of Glide API export gates into a single `switch (glide_export->ordinal)` Jump Table.
   - Zero string comparisons remaining in the Glide HLE gate dispatcher.

2. Verification
   - Debug build completed successfully.
   - 10-second smoke runtime test passed cleanly with 0 exceptions.
