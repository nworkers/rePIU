# Glide HLE 초기화 예외 0xe06d7363 분석 및 해결 설계
# Design for Analyzing and Resolving Glide HLE Initialization Exception 0xe06d7363

## 1. 개요 (Overview)

현재 `pumpit1` 타깃을 `aot-dynamic` 백엔드로 구동 시, 디코드 루프를 native 속도로 고속 패스한 직후 Glide 초기화 단계(`_GRSSTWINOPEN@28`)에서 C++ 예외 `0xe06d7363`이 발생하며 프로세스가 종료됩니다.
본 작업에서는 이 예외가 발생하는 정확한 지점(OpenGL 드라이버 부재로 인한 `wglCreateContext` 실패 또는 다른 라이브러리 예외)을 계측하고, 호스트에 3Dfx/OpenGL 가속 하드웨어가 없는 헤드리스 또는 VM 환경에서도 게임이 크래시 없이 진행될 수 있도록 Glide HLE 가짜/더미(Dummy) 초기화 폴백 모드를 설계합니다.

When running the `pumpit1` target with the `aot-dynamic` backend, immediately after passing the decode loop at native speeds, a C++ exception `0xe06d7363` is thrown during Glide initialization (`_GRSSTWINOPEN@28`), terminating the process.
This task designs diagnostic instrumentation to locate the exact source of this exception (e.g., `wglCreateContext` failure due to missing OpenGL drivers, or other library exceptions) and creates a dummy/stub Glide HLE fallback mode so the game can proceed without crashing even in virtualized or headless environments lacking 3Dfx/OpenGL acceleration hardware.

---

## 2. 분석 및 가설 (Analysis & Hypotheses)

### 가설 A: OpenGL 컨텍스트 생성 실패 및 C++ 예외 전파
- `GlideOpenGlBackend::OpenWindowed` 내에서 `wglCreateContext` 또는 `wglMakeCurrent`가 물리 GPU/드라이버가 없는 환경에서 실패하거나 내부적으로 C++ 예외를 던질 수 있습니다.
- 또는 `OpenWindowed`가 단순히 `false`를 반환했을 때, 게스트 프로그램(`PIU.EXE`) 측에서 실패를 감지하고 `RaiseException`을 통해 MSVC 계열의 C++ 예외(`0xe06d7363`)를 생성하여 비정상 종료를 시도할 수 있습니다. (하지만 게스트는 Watcom 컴파일러로 빌드되었으므로, 호스트 예외인 `0xe06d7363`을 던지려면 호스트 API나 라이브러리를 경유해야 합니다.)

### Hypothesis A: OpenGL Context Creation Failure & C++ Exception Propagation
- Inside `GlideOpenGlBackend::OpenWindowed`, `wglCreateContext` or `wglMakeCurrent` may fail or internally throw a C++ exception in environments lacking a physical GPU/driver.
- Alternatively, when `OpenWindowed` simply returns `false`, the guest program (`PIU.EXE`) might detect the failure and trigger an abort by calling a host API that raises the MSVC C++ exception (`0xe06d7363`). (However, since the guest is a Watcom-compiled binary, raising a host exception like `0xe06d7363` requires routing through a host API or library.)

---

## 3. 대응 설계 (Proposed Design)

### 3.1 1단계: 진단 계측 (Phase 1: Diagnostic Instrumentation)
- `_GRSSTWINOPEN@28` 처리 루틴(`HandleGlideGateBoundary`) 내부의 `OpenWindowed` 호출부를 `try-catch` 블록으로 감싸서 호스트 측 C++ 예외 발생 여부를 명시적으로 격리합니다.
- `OpenWindowed` 수행 후 결과(`opened`) 및 `glide_backend.message()` 내용을 표준 에러(`stderr`)로 즉시 출력하도록 진단 `printf`문을 삽입하여 오버헤드와 버퍼링 없이 즉시 터미널에서 확인할 수 있게 합니다.

### 3.2 2단계: 헤드리스/하드웨어 미지원 폴백(더미 모드) 구현 (Phase 2: Headless/No-Hardware Fallback / Dummy Mode)
- `OpenWindowed`가 GPU 환경 문제로 실패할 경우, 강제로 `opened = true` 상태를 합성하고 실제 창(window) 및 OpenGL 컨텍스트 없이도 내부 Glide 상태만 정상 설정되도록 하는 **Glide Dummy Backend Mode**를 도입합니다.
- Glide 그리기 명령들(`grDrawTriangle` 등)은 이미 HLE stub 처리가 되어 있으므로, 초기화만 성공(더미 컨텍스트)시켜 주면 게임 로직은 렌더링 카드 부재와 상관없이 메인 루프를 계속 진행할 수 있습니다.

### 3.1 Phase 1: Diagnostic Instrumentation
- Wrap the `OpenWindowed` call inside the `_GRSSTWINOPEN@28` handling routine (`HandleGlideGateBoundary`) in a `try-catch` block to explicitly isolate host-side C++ exceptions.
- Add diagnostic `printf` statements to immediately write the outcome (`opened`) and `glide_backend.message()` to `stderr` without buffering or spdlog pipeline overhead, enabling instant feedback in the terminal.

### 3.2 Phase 2: Headless/No-Hardware Fallback (Dummy Mode)
- If `OpenWindowed` fails due to GPU environment issues, introduce a **Glide Dummy Backend Mode** that forces `opened = true` and configures the internal Glide state successfully without creating a real window or active OpenGL context.
- Since drawing commands (e.g., `grDrawTriangle`) are already handled via HLE stubs, mock-initializing the graphics stack will allow the game logic to continue its main execution loop regardless of the missing hardware card.
