# 20260726-319 작업 지시: _GRFOGMODE@4 백엔드 연동 및 주석 복구 구현 / Work order: Restore _GRFOGMODE and comments

설계: [20260726-319-restore-grfogmode-and-comments.md](../design/20260726-319-restore-grfogmode-and-comments.md)

## 한국어

### 목표

`_GRFOGMODE@4`의 `context->glide_backend.SetFogMode` 연동 로직을 정식으로 복구하고, `_GRHINTS@8` 등 마이그레이션 과정에서 삭제되었던 주석을 완전 복원한다.

---

### 작업 내용

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `case go::kGrFogMode:` 핸들러 내에 `SetFogMode` 호출 및 실패 검증 추가.
   - `case go::kGrHints:` 위에 Glide 공식 스펙 기반 no-op 수용 의도 주석 복원.

2. 검증
   - Debug 빌드 (`cmake --build build --config Debug`).
   - 10초 스모크 런타임 테스트 실행.

---

## English

### Tasks

1. Restore `SetFogMode` backend invocation and error check in `case go::kGrFogMode:`.
2. Restore specification comments above `case go::kGrHints:`.
3. Verify via Debug build and 10s smoke runtime test.
