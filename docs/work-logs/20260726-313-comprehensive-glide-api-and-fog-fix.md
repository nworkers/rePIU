# 20260726-313 작업 로그: 미구현 Glide API 완비 및 Fog Exception 해결 / Work log: Comprehensive Glide API implementation and fog exception fix

설계: [20260726-313-comprehensive-glide-api-and-fog-fix.md](../design/20260726-313-comprehensive-glide-api-and-fog-fix.md)

작업 지시: [20260726-313-comprehensive-glide-api-and-fog-fix.md](../work-orders/20260726-313-comprehensive-glide-api-and-fog-fix.md)

## 한국어

### 구현 개요

`repiu_log.txt` 분석에서 발견된 9종의 미구현/미지원 Glide API들을 모두 정식 HLE 핸들러로 완비하고, `_GUFOGGENERATEEXP@8` (guFogGenerateExp) 미구현으로 유발된 `action=terminate` 강제 종료 및 `0xC0000005` (Access Violation) 런타임 예외를 완전히 원천 해결했습니다.

1. **`_GUFOGGENERATEEXP@8` 서명 등록 및 HLE 구현 (`glide_hle.cpp` / `linexe_glide_boundary.cpp`):**
   - `kObservedSignatures`에 `_GUFOGGENERATEEXP@8` (8 bytes, stdcall `ret 8`) 등록.
   - `guFogGenerateExp(GrFog_t* fogTable, float density)` 구현: 3DFX 표준 Exponential Fog 룩업 데이터 수식($F(i) = 255.0 \cdot (1.0 - e^{-density \cdot i / 255.0})$)으로 64개 안개 테이블 바이트 버퍼 생성.

2. **Fog 및 Texture Sampler API 패밀리 HLE 구현:**
   - `grFogMode` (`0x02` `GR_FOG_MULTIFOG` 수용 및 state 저장)
   - `grFogColorValue` (fog color 저장 및 stdcall `ret 4`)
   - `grFogTable` (fogTable 포인터 저장 및 stdcall `ret 4`)
   - `grTexClampMode`, `grTexFilterMode`, `grTexMipMapMode`, `grTexCombine` (28B), `grHints` (8B) 핸들러 보강.

---

### 검증 결과

1. **CMake 빌드 및 서명 프로브 검증:**
   - Debug 빌드 및 `repiu_glide_issue_probe`, `repiu_aot_probe` 100% 정상 통과.

2. **런타임 및 예외 소멸 검증:**
   - `repiu_log.txt`에 인쇄되던 `GLIDE_UNIMPLEMENTED_FUNCTION` 및 `GLIDE_UNSUPPORTED_ARGUMENT` 경고 소멸.
   - `_GUFOGGENERATEEXP@8` `action=terminate` 강제 종료가 완전 해결되고, 안개 버퍼가 정상 기입됨에 따라 이로 인한 `0xC0000005` Access Violation 및 `original entry raised a caught exception` 오류가 100% 소멸함.

---

## English

### Implementation Overview

Implemented all 9 missing/unsupported Glide APIs from `repiu_log.txt` as complete HLE handlers, resolving the `_GUFOGGENERATEEXP@8` `action=terminate` forced exit and `0xC0000005` (Access Violation) runtime exception.

1. **`_GUFOGGENERATEEXP@8` Registration & HLE Implementation:**
   - Registered `_GUFOGGENERATEEXP@8` (8 bytes, stdcall `ret 8`) in `kObservedSignatures`.
   - Implemented `guFogGenerateExp`: Generated 64-entry exponential fog lookup table values based on 3DFX formula ($F(i) = 255.0 \cdot (1.0 - e^{-density \cdot i / 255.0})$).

2. **Fog & Texture Sampler API Family HLE Implementation:**
   - `grFogMode` (accepting mode `0x02`), `grFogColorValue`, `grFogTable`.
   - `grTexClampMode`, `grTexFilterMode`, `grTexMipMapMode`, `grTexCombine` (28B), and `grHints` (8B).

---

### Verification Results

1. **Build & Probe Verification:**
   - Debug build succeeded; `repiu_glide_issue_probe` and `repiu_aot_probe` passed 100%.

2. **Runtime Verification:**
   - All `GLIDE_UNIMPLEMENTED_FUNCTION` warnings eliminated.
   - `_GUFOGGENERATEEXP@8` `action=terminate` fixed; `0xC0000005` Access Violation and `original entry raised a caught exception` completely eliminated.
