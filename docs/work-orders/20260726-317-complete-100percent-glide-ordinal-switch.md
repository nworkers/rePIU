# 20260726-317 작업 지시: Glide Boundary 전체 Ordinal Switch 완전 흡수 / Work order: Complete 100% Glide Ordinal Switch

설계: [20260726-317-complete-100percent-glide-ordinal-switch.md](../design/20260726-317-complete-100percent-glide-ordinal-switch.md)

## 한국어

### 목표

`linexe_glide_boundary.cpp` 내 630~674행의 로깅 체크 및 1800~2460행의 하단 LFB, Triangle, Polygon, ConstantColor 핸들러를 포함한 모든 `glide_export->name` 비교 구문을 단 1개도 남김없이 전면 삭제하고 단일 `switch (glide_export->ordinal)` 점프 테이블에 100% 흡수시킨다.

---

### 작업 내용

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - 630~674행 로깅 체크의 문자열 비교를 `ordinal` 비교(예: `ordinal == 9U`, `ordinal == 3U`, `ordinal == 49U` 등)로 전환.
   - `_GRDRAWTRIANGLE@12`, `_GRDRAWPLANARPOLYGON@12`, `_GRDRAWPOLYGON@12`, `_GRDRAWPLANARPOLYGONVERTEXLIST@8`, `_GRCONSTANTCOLORVALUE@4`, `_GRLFBLOCK@24`, `_GRLFBUNLOCK@8`, `_GRLFBWRITEREGION@32`, `_GRLFBREADREGION@28`, `_GRLFBCONSTANTALPHA@4`, `_GRLFBCONSTANTDEPTH@4`, `_GRLFBWRITECOLORSWIZZLE@8` 핸들러들을 단일 `switch (glide_export->ordinal)` 내부 케이스로 전부 수직 이동 및 통합.
   - `grep_search`로 파일 전체에 `glide_export->name ==` 검사 시 0개 확인.

2. 검증 및 프로파일링
   - CMake Debug 빌드 (`cmake --build build --config Debug`).
   - 10초 스모크 런타임 테스트 실행.

---

## English

### Objectives

Eliminate 100% of remaining `glide_export->name` string checks in `linexe_glide_boundary.cpp` by migrating logging checks and all trailing draw/LFB/polygon handlers into the single unified `switch (glide_export->ordinal)` jump table.

---

### Tasks

1. Update `linexe_glide_boundary.cpp` logging checks and move trailing handlers into `switch (glide_export->ordinal)`.
2. Confirm 0 occurrences of `glide_export->name ==` remain in `linexe_glide_boundary.cpp`.
3. Verify Debug build and run 10s runtime smoke test.
