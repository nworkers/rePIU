# 20260726-320 작업 로그: Glide Boundary 소실 주석 100% 완전 복원 / Work log: Restore all lost Glide boundary comments

작업 지시: [20260726-320-restore-all-glide-boundary-comments.md](../work-orders/20260726-320-restore-all-glide-boundary-comments.md)

## 한국어

### 작업 요약

`src/platform/win32/boundary/linexe_glide_boundary.cpp` 전체를 커밋 `1e4c660`과 1:1 정밀 대조하여, Glide 점프 테이블 마이그레이션 도중 누락되었던 15개 핵심 기능 영역의 30여 개 기술적 의도 및 스펙 설명 주석을 단 한 줄도 빠짐없이 **100% 완전 복원**하였습니다.

---

### 복원된 주석 세부 목록 (15개 기능 영역 전수)

1. **로깅/진단부:** Task 255 R3 texture/combine 실시간 observation 의도 주석 복원.
2. **`_GRHINTS@8`:** Glide 2.4 공식 스펙 기반 optimization advice 수용 및 stdcall ABI 유지 주석 복원.
3. **`_GRTEXDOWNLOADTABLE@12`:** Palette table (GR_TEXTABLE_PALETTE) 디코딩 및 ARGB->RGBA 픽셀 변환 설명 주석 복원.
4. **`_GRTEXDOWNLOADMIPMAPLEVEL@32`:** Texel 이미지 업로드/바인딩, Format census, BMP dump 의도 주석 복원.
5. **`_GRTEXSOURCE@16`:** current texture 바인딩 정책 주석 복원.
6. **`_GRTEXMINADDRESS@4` & `_GRTEXMAXADDRESS@4`:** fxTMInit 호출자 프레임 정리 및 stdcall ABI 상술 주석 복원.
7. **`_GRALPHACOMBINE@20` & `_GRCOLORCOMBINE@20`:** GLSL 트랜슬레이터 호환성, ABI 프레임 보호(Design 237), Combine function 3(GR_COMBINE_FUNCTION_SCALE_OTHER) 라우팅 주석 복원.
8. **`_GRALPHABLENDFUNCTION@16`:** 백엔드 미표현 알파 블렌드 식의 프레임 누수 방지 정책 주석 복원.
9. **`_GRCONSTANTCOLORVALUE@4`:** CONSTANT combine source 유지를 위한 상수 컬러 저장 정책 주석 복원.
10. **`_GRTEXCLAMPMODE@12`:** Texture sampler 파라미터 백엔드 처리 의도 주석 복원.
11. **`_GRDRAWTRIANGLE@12`:** GrVertex (60-byte 2-TMU) 구조 디코딩, Draw diagnostic, Triangle census 의도 주석 복원.
12. **`_GRLFBLOCK@24` & `_GRLFBUNLOCK@8`:** 565 staging surface seed 처리, GrLfbInfo_t layout/size echo 명세, blit 검증 및 픽셀 동기화 기술 주석 복원.

---

### 검증

- Debug 구성 빌드 성공 (`cmake --build build --config Debug`).
- 10초 스모크 런타임 테스트 정상 동작 확인 (예외 없이 타임아웃 종료).

---

## English

### Task Summary

Conducted a meticulous line-by-line audit of `linexe_glide_boundary.cpp` against commit `1e4c660` and reinstated **100% of all lost technical and specification comments** across 15 functional areas (30+ comment blocks).

---

### Verification

- Debug build passed cleanly.
- 10-second smoke runtime test completed successfully.
