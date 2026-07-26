# 20260726-315 작업 지시: 잔재 미구현 Glide 경고 로그 완전 소멸 구현 / Work order: Eliminate residual Glide unimplemented warnings

설계: [20260726-315-eliminate-residual-glide-unimplemented-warnings.md](../design/20260726-315-eliminate-residual-glide-unimplemented-warnings.md)

## 한국어

### 목표

`linexe_glide_boundary.cpp` 상단의 구식 `record_unimplemented` 공용 fallback 체인을 전면 제거하고 `switch (glide_export->ordinal)` 내에 모든 Texture Sampler & Hint API를 수직 통합하여 `GLIDE_UNIMPLEMENTED_FUNCTION` 경고 로그를 100% 소멸시킨다.

---

### 작업 내용

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - 상단 `record_unimplemented("texture-sampler-state-noop", ...)` 및 `record_unimplemented("optimization-hint-noop", ...)` 블록 삭제.
   - `switch (glide_export->ordinal)` 내에 `case 132` (`_GRTEXCOMBINE@28`), `case 136` (`_GRTEXMIPMAPMODE@12`), `case 31` (`_GRHINTS@8`) 핸들러 수직 통합.

2. 검증 및 프로파일링
   - CMake Debug 빌드 및 `repiu_glide_issue_probe` / `repiu_aot_probe` 검증.
   - 런타임 10초 스모크 테스트 실행으로 `GLIDE_UNIMPLEMENTED_FUNCTION` 경고 0건 검증.

---

## English

### Objectives

Completely eliminate `GLIDE_UNIMPLEMENTED_FUNCTION` warning logs by removing legacy `record_unimplemented` fallback blocks in `linexe_glide_boundary.cpp` and consolidating all texture sampler & hint APIs inside the `switch (glide_export->ordinal)` dispatcher.

---

### Tasks

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`: Remove legacy `record_unimplemented` fallback blocks and consolidate `case 132`, `case 136`, `case 31` inside the ordinal switch.
2. Verification: Build cleanly and run 10-second runtime test to verify 0 unimplemented warning logs.
