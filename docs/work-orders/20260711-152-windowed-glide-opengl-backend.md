# Windowed Glide OpenGL backend 작업 지시

1. export/gate metadata와 논리 Glide state를 `src/hle/glide_hle.cpp`로 분리합니다.
2. Win32 window/WGL/OpenGL 자원을 별도 backend header/source로 추가합니다.
3. `grSstWinOpen` ABI를 backend open command에 연결하고 FXTRUE/FXFALSE를 반환합니다.
4. 640×480 client window, double buffer, auxiliary/depth 요청과 message pump를 검증합니다.
5. 빌드 및 실행으로 다음 실제 Glide export와 renderer 의사결정 지점을 관찰합니다.
6. 대형 기능을 공용/플랫폼/ABI adapter 파일로 분리하는 원칙을 아키텍처 문서에 반영합니다.

# Windowed Glide OpenGL Backend Work Order

Extract export/gate metadata and logical Glide state into `src/hle/glide_hle.cpp`; add a separate Win32 WGL/OpenGL backend; connect `grSstWinOpen` ABI to backend creation and FXTRUE/FXFALSE; validate a 640×480 client window, double buffering, auxiliary/depth request, and message pumping; observe the next real Glide export; and document the large-feature file-boundary rule.
