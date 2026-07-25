# Work Log: Glide LFB 기능 개선 (Loader Hang/Black Screen 해결)

## 작업 요약 / Summary
PIU 구버전 로더나 동영상 재생 시 무한루프나 검은 화면이 발생하던 현상을 해결하기 위해 Glide LFB(Linear Frame Buffer) 경로를 대폭 보강했습니다.

## 구현 상세 / Implementation Details
1. **`grLfbLock`의 `FRONTBUFFER` 허용** (`linexe_glide_boundary.cpp`)
   - 기존에는 `kGlideBufferBackBuffer` 요청만 수락했으나, `kGlideBufferFrontBuffer`도 허용하도록 완화했습니다. 이로 인해 프론트 버퍼에 직접 그리기를 시도하는 로더가 FXFALSE를 받고 계속 재시도하여 무한루프(Hang)에 빠지던 문제를 해결했습니다.

2. **프론트 버퍼 렌더링 즉시 반영 (`glide_opengl_backend.cpp`)**
   - `PresentLfbSurface`에 `present_to_front` 파라미터를 추가했습니다.
   - 프론트 버퍼 Lock 해제(Unlock) 시 `present_to_front = true`를 넘겨받아 렌더링 직후 `SwapBuffers(0)`를 명시적으로 호출하게 하여, `grBufferSwap`이 없어도 화면이 갱신(검은 화면 탈출)되게 했습니다.

3. **`grLfbWriteRegion@32` 명시적 구현 (`glide_lfb.cpp`, `linexe_glide_boundary.cpp`)**
   - LFB 내부의 특정 영역(x, y, width, height)에 565 포맷의 픽셀을 덮어쓰는 `WriteRegionToGlideLfb565` 유틸리티 함수를 구현했습니다.
   - 게스트의 `grLfbWriteRegion` 호출 시 해당 함수를 사용해 Surface를 부분 갱신하고, 전체 픽셀을 디코딩하여 화면에 뿌려지도록 처리했습니다.

4. **미처리 LFB 함수에 대한 무한루프 방지 더미 핸들러 추가**
   - `grLfbReadRegion@28`
   - `grLfbConstantAlpha@4`
   - `grLfbConstantDepth@4`
   - `grLfbWriteColorSwizzle@8`
   - 위 함수들은 기능이 비어있으나 `FXTRUE` 또는 정상적인 스택 반환(cleanup)을 수행하게 하여 호출되더라도 로직이 멈추지 않게 방어했습니다.

## 결과 및 특이사항 / Results
- 빌드 검증: 모든 코드 변경이 정상적으로 컴파일 및 빌드(CMake MSBuild) 통과했습니다.
- 향후 최적화: `grLfbWriteRegion`이 빈번하게 발생할 경우 전체 Surface를 매번 디코딩하고 `PresentLfbSurface`를 호출하는 오버헤드가 발생할 수 있으므로, 프레임 최적화가 필요할 때 OpenGL `glTexSubImage2D` 등을 통한 부분 업데이트 최적화를 도입할 수 있습니다.
