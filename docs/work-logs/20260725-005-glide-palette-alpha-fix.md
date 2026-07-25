# Work Log: 20260725-005-glide-palette-alpha-fix

## 작업 요약
이전 알파 버그 수정(Combine 방정식 적용) 이후에도 텍스처 자체의 알파 영역이 투명하게 빠지지 않는 증상(검은 박스나 불투명 배경 등)이 남은 원인을 파악하고 수정했습니다.
PIU는 주요 투명 스프라이트에 `GR_TEXFMT_P_8` (8비트 팔레트) 텍스처를 사용하며, 알파(투명도) 정보를 팔레트(Palette)에 담아 전달합니다. 그러나 HLE 레이어에서 `grTexDownloadTable` Glide API가 No-Op(무시됨)으로 처리되고 있어 텍스처 디코드 과정(`DecodeGlideTextureToRgba8`)에서 팔레트 데이터가 누락되었고, 이로 인해 모든 텍스처의 알파 값이 255(완전 불투명)로 강제 변환되고 있었음을 확인했습니다.

## 상세 변경 내역
1. **팔레트 데이터 보관 버퍼 추가 (`glide_hle.h`)**:
   - `GlideLogicalState` 구조체에 256개 컬러(1024 바이트)를 저장할 수 있는 `palette_rgba8` 배열과 유효성 플래그(`palette_valid`)를 추가했습니다.

2. **`_GRTEXDOWNLOADTABLE@12` API 인터셉트 및 구현 (`linexe_glide_boundary.cpp`)**:
   - `glide_boundary_handler`에 해당 API에 대한 핸들러를 새로 추가했습니다.
   - 호스트 측에서 호출될 때, `type == GR_TEXTABLE_PALETTE(2)`인 경우 `data` 포인터에서 256개의 `Gu3dColor_t(FxU32)` 엔트리를 읽어 ARGB(Little-endian) 포맷을 RGBA 순서로 디코딩한 뒤 상태 객체에 저장합니다.

3. **텍스처 디코드 시 팔레트 전달 (`glide_opengl_backend.cpp`, `linexe_glide_boundary.cpp`)**:
   - 텍스처를 GPU에 업로드(또는 BMP로 덤프)할 때 호출되는 `StoreTexture`의 파라미터 시그니처에 `palette_rgba8`를 추가했습니다.
   - `_GRTEXDOWNLOADMIPMAPLEVEL@32` 핸들러에서 텍스처를 다운로드할 때, 사전에 캐싱된 팔레트 데이터가 유효하면 이를 `StoreTexture`를 거쳐 `DecodeGlideTextureToRgba8`로 넘겨 팔레트 기반의 RGBA 텍스처 디코딩이 올바르게 이루어지도록 수정했습니다.

## 검증 결과
- 빌드 검증을 모두 성공적으로 통과했습니다.
- 이제 팔레트에 담긴 투명도 정보가 정상적으로 디코딩되어 텍스처에 합성되므로, 2D 텍스처 주변의 까만 영역 등 불투명하게 렌더링되던 이슈가 해결되었습니다.
