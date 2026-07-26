# 20260726-316 작업 지시: Glide Boundary 전체 Ordinal Switch 단일 통합 구현 / Work order: Unified Glide Boundary Ordinal Switch

설계: [20260726-316-unified-glide-boundary-ordinal-switch.md](../design/20260726-316-unified-glide-boundary-ordinal-switch.md)

## 한국어

### 목표

`linexe_glide_boundary.cpp` 내의 모든 `glide_export->name` 문자열 비교문을 전면 삭제하고, 50여 개 Glide API 전체를 단일 `switch (glide_export->ordinal)` 점프 테이블로 100% 수직 통합한다.

---

### 작업 내용

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `_GRGLIDEINIT@0` (1), `_GRBUFFERCLEAR@12` (2), `_GRBUFFERSWAP@4` (3), `_GRSSTQUERYHARDWARE@4` (6), `_GRSSTSELECT@4` (7), `_GRSSTWINOPEN@28` (9), `_GRLFBLOCK@24` (47), `_GRLFBUNLOCK@8` (48), `_GRDRAWTRIANGLE@12` (52) 등 모든 API를 단일 `switch (glide_export->ordinal)`에 통합.
   - 문자열 비교 구문 0개 달성.

2. 검증 및 프로파일링
   - CMake Debug 빌드 및 `repiu_glide_issue_probe` / `repiu_aot_probe` 검증.
   - 런타임 10초 스모크 테스트 실행으로 정상 동작 확인.

---

## English

### Objectives

Refactor all 50+ Glide API handlers in `linexe_glide_boundary.cpp` into a single unified `switch (glide_export->ordinal)` O(1) Direct Jump Table, completely eliminating string comparison logic.

---

### Tasks

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`: Consolidate all Glide API checks into a single `switch (glide_export->ordinal)` block.
2. Verification: Build cleanly and run 10-second runtime test to verify clean execution.
