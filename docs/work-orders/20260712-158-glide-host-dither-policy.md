# Glide host dithering 정책 작업 지시

1. 공용 Glide 논리 상태에 dither mode를 추가합니다.
2. 312-byte state image Get/Set 직렬화에 dither mode를 포함합니다.
3. Win32 OpenGL backend에서 관찰된 mode 2를 `GL_DITHER` 활성화로 처리합니다.
4. typed signature와 guest ABI adapter를 연결합니다.
5. GLSL ordered dithering을 후순위 TODO로 analysis와 작업 로그에 기록합니다.
6. Win32 x86 Debug 빌드와 실제 asset 실행으로 다음 frontier를 확인합니다.
7. 아키텍처와 작업 로그를 갱신하고 커밋합니다.

# Glide Host Dithering Policy Work Order

Add shared dither state; include it in the 312-byte Get/Set image; map observed mode 2 to `GL_DITHER` in the Win32 backend; connect the typed signature and guest ABI adapter; record exact GLSL ordered dithering as a later TODO; build and run Win32 x86 to the next frontier; update architecture and the work log; and commit.
