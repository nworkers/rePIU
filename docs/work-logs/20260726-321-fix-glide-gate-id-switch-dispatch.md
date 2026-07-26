# 20260726-321 작업 로그: GlideGateId 기반 안전한 O(1) Switch Dispatch 구현 / Work log: Implement Safe GlideGateId Switch Dispatch

작업 지시: [20260726-321-fix-glide-gate-id-switch-dispatch.md](../work-orders/20260726-321-fix-glide-gate-id-switch-dispatch.md)

## 한국어

### 작업 요약

`repiu_log.txt` 실행 중발생한 `Error creating fxMesa context` 오류의 근본 원인을 규명하고 이를 완벽히 해결하였습니다.

- **원인 분석:** 게이트 트랩 바운더리가 `glide_export->ordinal` (바이너리 LE Resident Name Table export ordinal 값: 예 `_GRSSTWINOPEN@28` = `118`)을 switch 값으로 비교하였으나, `glide_hle.h`에 작성되어 있던 상수(`kGrSstWinOpen = 9U`)와의 불일치로 `_GRSSTWINOPEN@28`이 `default:` 미구현 핸들러로 빠졌습니다. 이로 인해 EAX 성공(1) 값이 설정되지 않아 `fxMesa` 컨텍스트 생성이 실패하였습니다.
- **해결 조치:**
  1. `include/repiu/hle/glide_hle.h`에 고유 `enum class GlideGateId : std::uint16_t`를 정의하고 `GlideExportGate`에 `gate_id` 필드를 추가하였습니다.
  2. `src/hle/glide_hle.cpp`에서 게이트 플랜 구성 시 게이트 이름 기반으로 `gate_id`를 1:1 확정 할당하는 `ResolveGlideGateId` 함수를 작성하였습니다.
  3. `src/platform/win32/boundary/linexe_glide_boundary.cpp`의 점프 테이블을 `switch (glide_export->gate_id)`로 전환하여 바이너리 오디널 숫자 변동에 완전히 독립적이고 문자열 비교가 없는 **100% 안전한 O(1) Dispatcher**를 완성하였습니다.

---

### 검증 결과

1. **컴파일 빌드:** Debug 구성 성공.
2. **런타임 검증:** 10초 스모크 런타임 테스트 결과, `_GRSSTWINOPEN@28`이 바르게 수용되고 `Error creating fxMesa context` 오류가 **완전히 제거**되었으며 렌더링/비디오 게이트가 정상 작동하여 타임아웃까지 안정 실행되었습니다.

---

## English

### Task Summary

Identified and resolved the root cause of `Error creating fxMesa context` in `repiu_log.txt`.

- **Root Cause:** Gate trap boundary relied on raw binary DLL export ordinals (e.g. `_GRSSTWINOPEN@28` = `118`), which mismatched with header constants (`9U`), causing `_GRSSTWINOPEN@28` to fall into `default:`.
- **Solution:** Introduced `enum class GlideGateId`, mapped `gate_id` during plan generation, and refactored the boundary jump table to `switch (glide_export->gate_id)`.
- **Verification:** Clean Debug build & 10s runtime smoke test passed without `fxMesa` errors.
