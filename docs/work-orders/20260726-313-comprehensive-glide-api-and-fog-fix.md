# 20260726-313 작업 지시: 미구현 Glide API 완비 및 Fog Exception 해결 구현 / Work order: Comprehensive Glide API implementation and fog exception fix

설계: [20260726-313-comprehensive-glide-api-and-fog-fix.md](../design/20260726-313-comprehensive-glide-api-and-fog-fix.md)

## 한국어

### 목표

`repiu_log.txt`에 나타난 9종의 미구현/미지원 Glide API들을 모두 정식 HLE 핸들러로 완비하여 `_GUFOGGENERATEEXP@8` `action=terminate` 강제 종료 및 `0xC0000005` (Access Violation) 런타임 예외를 완전 해결한다.

---

### 작업 내용

1. `include/repiu/hle/glide_hle.h`
   - Fog State 레지스터 필드 보강.

2. `src/hle/glide_hle.cpp`
   - `kObservedSignatures`에 `{"_GUFOGGENERATEEXP@8", 8U, GlideReturnKind::kVoid}` 추가.
   - `guFogGenerateExp` Exponential Fog Table 생성 로직 및 Fog 패밀리 (`grFogMode`, `grFogColorValue`, `grFogTable`) HLE 구현.
   - Texture Sampler & Hint 패밀리 (`grTexClampMode`, `grTexFilterMode`, `grTexMipMapMode`, `grTexCombine`, `grHints`) HLE 구현.

3. `src/platform/win32/boundary/linexe_glide_boundary.cpp`
   - Gate / Trampoline 내 `_GUFOGGENERATEEXP@8` 및 Glide API 패밀리 래퍼 연결.

4. 검증
   - Debug 빌드 및 `repiu_glide_issue_probe` / `repiu_aot_probe` 검증.
   - 60초 런타임 실행으로 `GLIDE_UNIMPLEMENTED_FUNCTION` 경고 소멸 및 `0xC0000005` 예외 완전 해결 검증.

---

## English

### Objectives

Implement all 9 missing/unsupported Glide APIs from `repiu_log.txt` as complete HLE handlers, eliminating `_GUFOGGENERATEEXP@8` `action=terminate` forced exits and `0xC0000005` (Access Violation) runtime exceptions.

---

### Tasks

1. `include/repiu/hle/glide_hle.h`: Extend Fog State fields.
2. `src/hle/glide_hle.cpp`: Add `_GUFOGGENERATEEXP@8` signature, implement `guFogGenerateExp` table generation, and implement Fog / Texture Sampler HLE handlers.
3. `src/platform/win32/boundary/linexe_glide_boundary.cpp`: Connect `_GUFOGGENERATEEXP@8` and Glide API wrappers in the Gate/Trampoline.
4. Verification: Build, run probes, and verify zero unimplemented warnings and zero `0xC0000005` exceptions in a 60-second runtime run.
