# SDL3 창 및 CD-DA 오디오 전환 작업 지시

1. SDL3 의존성을 CMake에 고정 버전으로 추가하고 라이선스를 기록한다.
2. SDL3 OpenGL 창/컨텍스트/event backend를 별도 header/source로 구현한다.
3. WGL 및 Win32 message-pump 코드를 SDL3 API로 교체하고 GLSL loader를 `SDL_GL_GetProcAddress`로 전환한다.
4. `CdAudioWaveOut`를 SDL3 `SDL_AudioStream` 기반 구현으로 교체한다.
5. execution trampoline 및 MSCDEX adapter의 guest-visible 상태 계약을 유지한 채 새 backend를 연결한다.
6. Win32 x86 Debug 빌드와 `pumpit1` 관측으로 창, 렌더링, CD-DA 상태, 종료를 검증한다.
7. `ARCHITECTURE.md`, analysis/KB, 작업 로그를 갱신하고 하나의 작업 단위로 커밋한다.

## 완료 조건

`winmm` 및 직접 WGL/User32 창 처리 없이 SDL3가 창·이벤트·OpenGL 컨텍스트·CD-DA PCM 출력을 제공하고, 원본 PIU의 Glide/MSCDEX HLE ABI가 유지되어야 한다.

# SDL3 Window and CD-DA Audio Migration Work Order

1. Add a pinned SDL3 CMake dependency and record its license.
2. Implement dedicated SDL3 OpenGL window/context/event backend files.
3. Replace WGL and direct Win32 message pumping with SDL3 APIs and use `SDL_GL_GetProcAddress` for GLSL loading.
4. Replace `CdAudioWaveOut` with an SDL3 `SDL_AudioStream` implementation.
5. Connect the new backend while preserving execution-trampoline and MSCDEX guest-visible state contracts.
6. Validate Win32 x86 Debug build, real `pumpit1` rendering and CD-DA status, and shutdown.
7. Update architecture, analysis/KB, work log, and commit the task.
