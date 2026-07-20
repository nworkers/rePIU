# 20260721-258-texture-format-check

## 개요 (Overview)

Glide API를 통해 주입되는 텍스처 포맷이 OpenGL 백엔드가 지원 및 디코딩할 수 있는 형식인지 검사하고 거부하는 사전 검증 루틴을 구현합니다.
Implement a validation routine to check and reject incoming Glide texture formats that are unsupported or cannot be decoded by our OpenGL backend.

---

## 설계 및 제어 흐름 (Design & Control Flow)

```mermaid
flowchart TD
    A[GrTextureFormat_t passed to StoreTexture] --> B{Is format acceptable?}
    B -- No --> C[Set error: "unacceptable Glide texture format"]
    C --> D[Return false early]
    B -- Yes --> E[Decode Glide texture to RGBA8]
    E --> F[glTexImage2D and upload]
    F --> G[Return success]
```

허용되는 Glide 포맷(Acceptable Formats):
- `0 (RGB_332)`
- `2 (ALPHA_8)`
- `3 (INTENSITY_8)`
- `4 (ALPHA_INTENSITY_44)`
- `5 (P_8)`
- `8 (ARGB_8332)`
- `10 (RGB_565)`
- `11 (ARGB_1555)`
- `12 (ARGB_4444)`
- `13 (ALPHA_INTENSITY_88)`
- `14 (AP_88)`

---

## 작업 항목 (Tasks)

1. **glide_texture_decode.h & glide_texture_decode.cpp 수정**:
   - `IsGlideTextureFormatAcceptable(std::uint32_t format)` 함수 정의.
   - 위의 허용 포맷 리스트에 해당하는지 판단하여 bool 반환.
2. **glide_opengl_backend.cpp 수정**:
   - `GlideOpenGlBackend::StoreTexture` 시작 부분에 `IsGlideTextureFormatAcceptable` 호출 검증 코드 삽입.
   - 불합격 시 즉시 `false` 및 해당 오류 메시지 반환.
3. **빌드 및 검증**:
   - 정상 빌드 확인 및 `pumpit1` 기동 확인.

---

## Tasks (English)

1. **Modify glide_texture_decode.h & glide_texture_decode.cpp**:
   - Declare and implement `bool IsGlideTextureFormatAcceptable(std::uint32_t format);`.
   - Validate format against the list of 11 supported Glide texture formats.
2. **Modify glide_opengl_backend.cpp**:
   - Inject the format checking routine into the entry point of `GlideOpenGlBackend::StoreTexture`.
   - Return `false` early with an error message `"unacceptable Glide texture format"` if check fails.
3. **Build & Verification**:
   - Confirm successful build and verify execution stability on `pumpit1`.
