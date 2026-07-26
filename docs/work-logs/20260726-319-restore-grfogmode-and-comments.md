# 20260726-319 작업 로그: _GRFOGMODE@4 백엔드 연동 및 주석 복구 / Work log: Restore _GRFOGMODE and comments

작업 지시: [20260726-319-restore-grfogmode-and-comments.md](../work-orders/20260726-319-restore-grfogmode-and-comments.md)

## 한국어

### 작업 요약

`linexe_glide_boundary.cpp`의 switch 리팩토링 도중 누락되었던 `_GRFOGMODE@4` (`case go::kGrFogMode:`)의 백엔드 연동 로직 (`context->glide_backend.SetFogMode(mode)` 및 `decline_gate("fog-mode-backend-failure")`)을 완전하게 복원하고, `_GRHINTS@8` 위에 기술적 의도 설명 주석을 완벽히 복구하였습니다.

---

### 변경 세부 사항

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `case go::kGrFogMode:` 에 `context->glide_backend.SetFogMode(mode)` 호출 및 `decline_gate("fog-mode-backend-failure")` 예외 처리 추가.
   - `case go::kGrHints:` 위에 `// Glide documents this call as optimization advice. Preserve the observed stdcall ABI while the renderer has no verified hint policy.` 주석 복구.

2. 검증
   - Debug 빌드 정상 수용 (`cmake --build build --config Debug`).
   - 10초 스모크 런타임 테스트 통과.

---

## English

### Task Summary

Restored the missing `_GRFOGMODE@4` (`case go::kGrFogMode:`) backend integration logic (`context->glide_backend.SetFogMode(mode)` and error checking) in `linexe_glide_boundary.cpp`, and fully reinstated specification comments (e.g. `_GRHINTS@8`).

---

### Key Changes

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - Re-added `SetFogMode` backend invocation and `decline_gate` check inside `case go::kGrFogMode:`.
   - Restored original technical comment above `case go::kGrHints:`.

2. Verification
   - Clean Debug build.
   - 10-second smoke runtime test passed.
