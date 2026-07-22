# SDL3 OpenGL 함수 해석 작업 로그

GLSL backend의 `wglGetProcAddress`와 Win32 header 의존성을 `SDL_GL_GetProcAddress`, SDL3 OpenGL header로 교체했습니다. Win32 x86 Debug 단일 병렬 빌드가 성공했습니다. 이 변경은 SDL3가 소유할 GL context에서도 shader entry point 해석이 같은 API 계층을 사용하도록 준비합니다.

# SDL3 OpenGL Procedure Resolver Work Log

Replaced `wglGetProcAddress` and Win32 headers in the GLSL backend with `SDL_GL_GetProcAddress` and SDL3 OpenGL headers. The serialized Win32 x86 Debug build succeeded. This prepares shader resolution for an SDL3-owned GL context.
