# 20260726-320 작업 지시: Glide Boundary 소실 주석 전수 복원 구현 / Work order: Restore all lost Glide boundary comments

설계: [20260726-320-restore-all-glide-boundary-comments.md](../design/20260726-320-restore-all-glide-boundary-comments.md)

## 한국어

### 목표

`src/platform/win32/boundary/linexe_glide_boundary.cpp` 내에서 마이그레이션 도중 누락되었던 15개 영역의 모든 주석을 100% 원형대로 복원한다.

---

### 작업 내용

1. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - `git diff 1e4c660..HEAD` 분석으로 파악된 15개 영역의 30여 개 주석 단락 전수 복구.
   - 로깅, TexDownloadTable, TexDownloadMipMapLevel, TexSource, TexMin/MaxAddress, AlphaCombine, ColorCombine, AlphaBlend, DrawTriangle, ConstantColor, LfbLock, LfbUnlock 주석 완전 원상 복구.

2. 검증
   - Debug 빌드 (`cmake --build build --config Debug`).
   - 10초 스모크 런타임 테스트 실행.

---

## English

### Tasks

1. Re-insert all 30+ comment blocks across 15 areas in `linexe_glide_boundary.cpp`.
2. Build Debug configuration and verify via 10s smoke test.
