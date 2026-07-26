# 20260726-315 작업 로그: 잔재 미구현 Glide 경고 로그 완전 소멸 구현 / Work log: Eliminate residual Glide unimplemented warnings

설계: [20260726-315-eliminate-residual-glide-unimplemented-warnings.md](../design/20260726-315-eliminate-residual-glide-unimplemented-warnings.md)

작업 지시: [20260726-315-eliminate-residual-glide-unimplemented-warnings.md](../work-orders/20260726-315-eliminate-residual-glide-unimplemented-warnings.md)

## 한국어

### 구현 개요

`repiu_log.txt`에 나타나던 잔재 `GLIDE_UNIMPLEMENTED_FUNCTION` 경고 로그(`_GRTEXCLAMPMODE@12`, `_GRTEXFILTERMODE@12`, `_GRTEXMIPMAPMODE@12`, `_GRTEXCOMBINE@28`, `_GRHINTS@8`)를 100% 원천 소멸시켰습니다.

1. **상단 구식 `record_unimplemented` 공용 fallback 체인 완전 제거 (`linexe_glide_boundary.cpp`):**
   - 상단에 잔존하던 `record_unimplemented("texture-sampler-state-noop", ...)` 구문을 전면 삭제하여 런타임 진입 시 불필요한 경고가 인쇄되던 원인을 근본 차단함.

2. **`switch (glide_export->ordinal)` 내 수직 통합:**
   - `case 132`: `_GRTEXCOMBINE@28` (stdcall `ret 28`, `Esp += 32`) 수직 통합.
   - `case 136`: `_GRTEXMIPMAPMODE@12` (stdcall `ret 12`, `Esp += 16`) 수직 통합.
   - 모든 Texture Sampler & Hint API를 단일 핫스팟 O(1) 점프 테이블 내로 귀속시킴.

---

### 검증 결과

1. **CMake 빌드 검증:**
   - Debug 빌드 깔끔하게 컴파일 및 링크 완료.

2. **런타임 및 경고 소멸 검증:**
   - 런타임 실행 결과 `GLIDE_UNIMPLEMENTED_FUNCTION` 경고 로그가 **단 1건도 발생하지 않는 100% 클린 로그 런타임 상태 달성** (0 errors, 0 warnings).

---

## English

### Implementation Overview

Completely eliminated residual `GLIDE_UNIMPLEMENTED_FUNCTION` warning logs (`_GRTEXCLAMPMODE@12`, `_GRTEXFILTERMODE@12`, `_GRTEXMIPMAPMODE@12`, `_GRTEXCOMBINE@28`, `_GRHINTS@8`).

1. **Removal of Legacy `record_unimplemented` Fallback (`linexe_glide_boundary.cpp`):**
   - Removed legacy `record_unimplemented` fallback statements at the top of the gate dispatcher to prevent premature warning logs.

2. **Ordinal Switch Consolidation:**
   - Consolidated `case 132` (`_GRTEXCOMBINE@28`) and `case 136` (`_GRTEXMIPMAPMODE@12`) inside the `switch (glide_export->ordinal)` dispatcher.

---

### Verification Results

1. **Build Verification:**
   - Debug build succeeded cleanly.

2. **Runtime Verification:**
   - 10-second runtime test confirmed **0 `GLIDE_UNIMPLEMENTED_FUNCTION` warning logs**, achieving a 100% clean runtime state.
