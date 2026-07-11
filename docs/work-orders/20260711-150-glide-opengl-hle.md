# Glide 2 OpenGL HLE 작업 지시

1. OVL resident-name/ordinal 및 decorated ABI parser를 공용 LE 분석 계층에 추가합니다.
2. LINEXE virtual module handle과 동적 Glide trap gate를 구현합니다.
3. `GETPROCADDR`와 최초 호출을 trace하여 PIU의 실제 API 집합과 ABI를 확정합니다.
4. 플랫폼 공용 Glide state/render-command interface를 정의합니다.
5. Win32 OpenGL init/clear/swap 최소 backend부터 단계적으로 구현합니다.
6. triangle, render state, texture, LFB 순서로 확장하고 frame 검증을 추가합니다.

# Glide 2 OpenGL HLE Work Order

Parse OVL resident names, ordinals, and decorated ABI metadata; implement a virtual LINEXE module and dynamic Glide trap gates; trace PIU's actual API/ABI; define a platform-neutral Glide state and render-command interface; add a minimal Win32 OpenGL initialization/clear/swap backend; then expand through triangles, state, textures, and LFB with frame validation.
