# 20260721-257-texture-bmp-dump

## 작업 결과 요약 (Summary of Work Results)

* **목표 달성**: 텍스처 로딩 시점에서 버퍼 데이터를 BMP 파일로 자동 덤프하는 진단 기능을 성공적으로 추가했습니다.
* **구현 세부사항**:
  - `linexe_glide_boundary.cpp` 내부의 `_GRTEXDOWNLOADMIPMAPLEVEL@32` 핸들러에 `REPIU_DUMP_TEXTURE_BMP` 환경변수 검사 루틴을 추가했습니다.
  - RGBA8 데이터를 32비트 BGRA BMP(top-down 레이아웃)로 변환해주는 `DumpTextureToBmp` 헬퍼 함수를 추가하고, `DecodeGlideTextureToRgba8`를 사용하여 디코딩된 이미지를 `build/texture_dumps/`에 고유 파일명으로 저장하도록 했습니다.
* **검증 완료**: AOT-dynamic 실행 하에서 100초 타임아웃 테스트를 진행하여 2개의 1x1 텍스처 덤프 파일(`tex_0x0_fmt10_1x1_1.bmp`, `tex_0x8_fmt12_1x1_2.bmp`)이 정확히 58바이트 크기로 저장됨을 확인했습니다.

---

## 작업 결과 세부사항 (Detailed Work Results)

### 구현 사항
1. **linexe_glide_boundary.cpp** 수정
   - `<fstream>` 및 `<filesystem>` 헤더를 포함했습니다.
   - 32비트 BMP 파일 작성을 위한 구조체 정의 및 파일 쓰기 함수 `DumpTextureToBmp`를 익명 네임스페이스 내에 구현했습니다.
   - `_GRTEXDOWNLOADMIPMAPLEVEL@32` 핸들러에서 환경변수 `REPIU_DUMP_TEXTURE_BMP=1`일 때 텍스처를 덤프하도록 코드를 주입했습니다.

### 검증 과정
AOT-dynamic 컴파일러 모드(`REPIU_EXECUTION_BACKEND=aot-dynamic`)는 초기 파싱 루프(15~77초)를 빠르게 통과하므로, 100초 타임아웃 제한으로 텍스처 다운로드 진입 시점(78초 이후)을 안전하게 포착할 수 있었습니다.
실행 결과 `build/texture_dumps/` 폴더 하위에 두 개의 텍스처(RGB565, ARGB4444 포맷)가 BMP 파일로 정확하게 디코딩 및 덤프되었습니다.

---

## Summary of Work Results (English)

* **Goal Achieved**: Successfully implemented a runtime diagnostic feature to copy, decode, and dump texture buffer data into BMP files during loading.
* **Implementation Details**:
  - Gated the feature with the environment variable `REPIU_DUMP_TEXTURE_BMP`.
  - Added the helper function `DumpTextureToBmp` inside `linexe_glide_boundary.cpp` to convert RGBA8 to 32-bit BGRA (top-down BMP layout).
  - Used `DecodeGlideTextureToRgba8` to get clean RGBA8 bytes and write BMP dumps with uniquely formatted names to `build/texture_dumps/`.
* **Verification Completed**: Ran the loader with a 100s timeout under the AOT-dynamic execution backend and verified two 1x1 textures (`tex_0x0_fmt10_1x1_1.bmp`, `tex_0x8_fmt12_1x1_2.bmp`) were successfully written as 58-byte BMP files.
