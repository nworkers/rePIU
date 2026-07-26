# 20260726-320 Glide Boundary 소실 주석 전수 완전 복원 설계 / Design: Complete Restoration of Lost Comments

## 한국어

### 개요

이전 Glide Boundary 점프 테이블 리팩토링 과정에서 소실되었던 모든 주요 기술적 의도 및 스펙 설명 주석(15개 영역, 30여 개 단락 주석)을 100% 완전 복원하여 코드의 유지보수성과 도메인 지식 연속성을 회복합니다.

---

### 복원 대상 주요 주석 영역

1. **로깅/진단부 (lines 660~670):** Texture/Combine 게이트 실시간 아규먼트 dump 진단 의도 주석 복원.
2. **`_GRTEXDOWNLOADTABLE@12` (`go::kGrTexDownloadTable`):** Palette table (type 2 / GR_TEXTABLE_PALETTE) 디코딩 및 ARGB->RGBA 픽셀 변환 설명 주석 복원.
3. **`_GRTEXDOWNLOADMIPMAPLEVEL@32` (`go::kGrTexDownloadMipMapLevel`):** Texel 이미지 디코드/업로드, Format census, BMP dump 의도 주석 복원.
4. **`_GRTEXSOURCE@16` (`go::kGrTexSource`):** 텍스처 바인딩 및 텍스처 메모리 매핑 의도 주석 복원.
5. **`_GRTEXMINADDRESS@4` & `_GRTEXMAXADDRESS@4`:** fxTMInit 호출자 프레임 정리 및 stdcall ABI 상술 주석 복원.
6. **`_GRALPHACOMBINE@20` & `_GRCOLORCOMBINE@20`:** GLSL 트랜슬레이터 호환성, ABI 프레임 보호(Design 237), Combine function 3(GR_COMBINE_FUNCTION_SCALE_OTHER) 라우팅 주석 복원.
7. **`_GRALPHABLENDFUNCTION@16`:** 백엔드 미표현 알파 블렌드 식의 프레임 누수 방지 정책 주석 복원.
8. **`_GRDRAWTRIANGLE@12`:** GrVertex (60-byte 2-TMU) 구조 디코딩, Draw diagnostic, Triangle census 의도 주석 복원.
9. **`_GRCONSTANTCOLORVALUE@4`:** CONSTANT combine source 유지를 위한 상수 컬러 저장 정책 주석 복원.
10. **`_GRLFBLOCK@24` & `_GRLFBUNLOCK@8`:** 565 staging surface seed 처리, GrLfbInfo_t layout/size echo 명세, blit 검증 및 픽셀 동기화 기술 주석 100% 전수 복원.

---

## English

### Overview

Restores 100% of technical rationale and specification comments (spanning 15 functional areas) that were unintentionally lost during the Glide Boundary jump table refactoring.
