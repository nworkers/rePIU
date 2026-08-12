# Glide LFB region 전송 작업 지시

1. `include/repiu/hle/glide_lfb.h`의 `kGlideLfbSrcFmt565= 1`을 제거하고, 사양에 맞는
   `GrLfbSrcFmt_t` 상수 집합을 새 `glide_lfb_region` header로 옮깁니다.
2. `include/repiu/hle/glide_lfb_region.h`와 `src/hle/glide_lfb_region.cpp`를 추가하고
   포맷 표(`GlideLfbSrcFormatBytesPerPixel`, `GlideLfbSrcFormatSupported`), stride
   유도(`ResolveGlideLfbRegionStride`), 사각형 클립, `WriteGlideLfbRegion`,
   `ReadGlideLfbRegion`을 그 안에 둡니다. surface의 565 색 순서는 인자로 받습니다.
3. `WriteRegionToGlideLfb565`를 제거하고 boundary 호출을 `WriteGlideLfbRegion`으로
   바꿉니다.
4. thread context에 `glide_lfb_region_shadow_valid`,
   `glide_lfb_region_shadow_dirty`와 seed/present 계수를 추가합니다.
5. `linexe_glide_boundary.cpp`에 shadow ensure/flush/invalidate를 추가하고, region
   gate가 아닌 모든 gate 진입 전에 flush 후 invalidate하도록 draw batch flush 훅
   앞에 배치합니다.
6. `kGrLfbReadRegion`을 구현합니다. 버퍼 종류와 목적지 guest 범위를 검사하고, origin이
   lower-left면 행을 뒤집어 shadow에서 목적지로 복사합니다. 목적지 stride가 폭보다
   넓으면 행 단위로 써서 여백을 보존합니다.
7. `kGrLfbWriteRegion`을 모든 색 포맷(565/555/1555/888/8888)과 stride 0에 대응하도록
   일반화하고, 성공 시 shadow만 갱신하고 dirty로 표시합니다. 깊이 계열, `RLE16`, 음수
   stride는 `GLIDE_UNSUPPORTED_ARGUMENT`로 남깁니다. 두 gate 모두 반환값은 기존
   정책대로 `FXTRUE`를 유지합니다(work order 002: LFB 계열의 `FXFALSE` 반환이 guest를
   멈추게 했음).
8. `src/tools/aot_probe/glide_lfb_region_probe.{h,cpp}`를 추가하고 `main.cpp`와
   `CMakeLists.txt`에 등록합니다. 설계의 검증 항목 6가지를 모두 확인합니다.
9. Win32 x86 Debug 빌드를 수행하고 `aot_probe`를 실행합니다.
10. 설계·분석·kb 문서를 갱신하고 작업 로그를 남긴 뒤 하나의 작업 커밋으로 정리합니다.
11. 사용자 구동 결과로 방향과 색을 확인합니다. 어긋나면 좌표계와 색 순서 가정을
    고칩니다. (실행 결과 두 가정 모두 반증되어 수정했습니다. region `y`는 origin
    상대가 아니고, source word의 채널 순서는 `grLfbWriteColorFormat`을 따릅니다.)

# Glide LFB Region Transfer Work Order

1. Remove `kGlideLfbSrcFmt565 = 1` from `include/repiu/hle/glide_lfb.h` and move a
   specification-correct `GrLfbSrcFmt_t` constant set into the new
   `glide_lfb_region` header.
2. Add `include/repiu/hle/glide_lfb_region.h` and `src/hle/glide_lfb_region.cpp`
   holding the format table (`GlideLfbSrcFormatBytesPerPixel`,
   `GlideLfbSrcFormatSupported`), stride derivation
   (`ResolveGlideLfbRegionStride`), rectangle clipping, `WriteGlideLfbRegion`, and
   `ReadGlideLfbRegion`, taking the surface's 565 color order as an argument.
3. Remove `WriteRegionToGlideLfb565` and switch the boundary call to
   `WriteGlideLfbRegion`.
4. Add `glide_lfb_region_shadow_valid`, `glide_lfb_region_shadow_dirty`, and
   seed/present counters to the thread context.
5. Add shadow ensure/flush/invalidate to `linexe_glide_boundary.cpp`, flushing and
   then invalidating before every gate that is not a region gate, placed ahead of
   the draw-batch flush hook.
6. Implement `kGrLfbReadRegion`: validate the buffer kind and the destination
   guest range, flip the row index when the origin is lower-left, and copy from
   the shadow into the destination. Write row by row so a destination stride
   wider than the rectangle keeps its padding.
7. Generalize `kGrLfbWriteRegion` to every color source format
   (565/555/1555/888/8888) and to a zero stride, updating only the shadow and
   marking it dirty on success. Leave depth formats, `RLE16`, and negative
   strides reported as `GLIDE_UNSUPPORTED_ARGUMENT`. Both gates keep returning
   `FXTRUE` as before — work order 002 recorded that an `FXFALSE` from the LFB
   family stalls the guest.
8. Add `src/tools/aot_probe/glide_lfb_region_probe.{h,cpp}` and register it in
   `main.cpp` and `CMakeLists.txt`, covering all six design verification items.
9. Run the Win32 x86 Debug build and execute `aot_probe`.
10. Update the design, analysis, and knowledge-base documents, leave a work log,
    and land one task commit.
11. Check orientation and color against the user's run, and correct the
    coordinate and color-order assumptions if they disagree. (They did: region
    `y` is not origin-relative, and the source word's channel order follows
    `grLfbWriteColorFormat`.)
