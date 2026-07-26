# 20260726-318 Glide Ordinal 심볼릭 상수 도입 설계 / Design: Glide Ordinal Symbolic Constants

## 한국어

### 개요

`linexe_glide_boundary.cpp` 내의 `switch (glide_export->ordinal)` 및 ordinal 조건문에서 하드코딩된 숫자 리터럴(1, 2, 3, 9, 49, 132 등)을 사용함으로 인한 가독성 저하를 해결하기 위해, `include/repiu/hle/glide_hle.h`에 **타입 안전한 C++ `constexpr std::uint16_t` Glide Ordinal 심볼릭 상수 네임스페이스 (`repiu::hle::glide_ordinal`)**를 신설하고 모든 디스패처/로깅 매핑 코드를 심볼릭 상수로 전면 교체합니다.

---

### 심볼릭 상수 정의 체계 (`include/repiu/hle/glide_hle.h`)

```cpp
namespace repiu::hle::glide_ordinal {

constexpr std::uint16_t kGrGlideInit = 1U;
constexpr std::uint16_t kGrBufferClear = 2U;
constexpr std::uint16_t kGrBufferSwap = 3U;
constexpr std::uint16_t kGrBufferNumPending = 4U;
constexpr std::uint16_t kGrSstQueryHardware = 6U;
constexpr std::uint16_t kGrSstSelect = 7U;
constexpr std::uint16_t kGrSstWinClose = 8U;
constexpr std::uint16_t kGrSstWinOpen = 9U;
constexpr std::uint16_t kGrSstScreenWidth = 10U;
constexpr std::uint16_t kGrSstScreenHeight = 11U;
constexpr std::uint16_t kGrTexMinAddress = 12U;
constexpr std::uint16_t kGrTexMaxAddress = 13U;
constexpr std::uint16_t kGuFogGenerateExp = 16U;
constexpr std::uint16_t kGrHints = 31U;
constexpr std::uint16_t kGrColorMask = 32U;
constexpr std::uint16_t kGrRenderBuffer = 33U;
constexpr std::uint16_t kGrDepthMask = 34U;
constexpr std::uint16_t kGrDepthBiasLevel = 35U;
constexpr std::uint16_t kGrDepthBufferMode = 36U;
constexpr std::uint16_t kGrLfbWriteColorFormat = 37U;
constexpr std::uint16_t kGrAlphaCombine = 38U;
constexpr std::uint16_t kGrColorCombine = 39U;
constexpr std::uint16_t kGrAlphaBlendFunction = 40U;
constexpr std::uint16_t kGrAlphaTestFunction = 41U;
constexpr std::uint16_t kGrAlphaTestReferenceValue = 42U;
constexpr std::uint16_t kGrDepthBufferFunction = 43U;
constexpr std::uint16_t kGrClipWindow = 45U;
constexpr std::uint16_t kGrTexTextureMemRequired = 46U;
constexpr std::uint16_t kGrGlideGetState = 47U;
constexpr std::uint16_t kGrGlideSetState = 48U;
constexpr std::uint16_t kGrTexDownloadMipMapLevel = 49U;
constexpr std::uint16_t kGrDrawLine = 50U;
constexpr std::uint16_t kGrDrawPoint = 51U;
constexpr std::uint16_t kGrDrawTriangle = 52U;
constexpr std::uint16_t kGrDrawPlanarPolygon = 53U;
constexpr std::uint16_t kGrDrawPlanarPolygonVertexList = 54U;
constexpr std::uint16_t kGrDrawPolygon = 55U;
constexpr std::uint16_t kGrConstantColorValue = 66U;
constexpr std::uint16_t kGrLfbLock = 70U;
constexpr std::uint16_t kGrLfbUnlock = 71U;
constexpr std::uint16_t kGrLfbWriteRegion = 72U;
constexpr std::uint16_t kGrLfbReadRegion = 73U;
constexpr std::uint16_t kGrLfbConstantAlpha = 74U;
constexpr std::uint16_t kGrLfbConstantDepth = 75U;
constexpr std::uint16_t kGrTexDownloadTable = 76U;
constexpr std::uint16_t kGrLfbWriteColorSwizzle = 77U;
constexpr std::uint16_t kGrCullMode = 99U;
constexpr std::uint16_t kGrDitherMode = 100U;
constexpr std::uint16_t kGrFogMode = 101U;
constexpr std::uint16_t kGrFogColorValue = 102U;
constexpr std::uint16_t kGrFogTable = 103U;
constexpr std::uint16_t kGrTexClampMode = 131U;
constexpr std::uint16_t kGrTexCombine = 132U;
constexpr std::uint16_t kGrTexFilterMode = 134U;
constexpr std::uint16_t kGrTexMipMapMode = 136U;
constexpr std::uint16_t kGrTexSource = 138U;

} // namespace repiu::hle::glide_ordinal
```

---

### 디스패처 적용 예시 (`linexe_glide_boundary.cpp`)

```cpp
namespace go = repiu::hle::glide_ordinal;

switch (glide_export->ordinal)
{
    case go::kGrGlideInit:
        // ...
    case go::kGrSstWinOpen:
        // ...
    case go::kGrDrawTriangle:
        // ...
}
```

---

## English

### Overview

Replaces hardcoded integer ordinal literals (1, 2, 3, 9, 49, 132, etc.) in `linexe_glide_boundary.cpp` with a new, type-safe **`constexpr std::uint16_t` symbolic constants namespace (`repiu::hle::glide_ordinal`)** in `include/repiu/hle/glide_hle.h`, dramatically improving code readability and maintainability.
