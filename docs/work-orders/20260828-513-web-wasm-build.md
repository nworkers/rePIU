# 웹(WebAssembly) 빌드 구성 작업 지시 (Stage 1)

설계: [20260828-513-web-wasm-execution.md](../design/20260828-513-web-wasm-execution.md)

1. `scripts/build_web_wasm.sh`를 추가합니다. `build_linux_i386.sh`와 같은 인자 형태
   (`--config`, `--target`)를 쓰고 `build/web_wasm`에 빌드합니다. emsdk가 없거나
   `emcmake`가 PATH에 없으면 **설치·활성화 명령을 이름 붙여 안내하고 종료**합니다.
   툴체인 부재가 수백 개의 헤더 오류로 묻히면 안 됩니다.
2. `CMakeLists.txt`에 `if(EMSCRIPTEN)` 분기를 넣습니다. Emscripten 구성에서는
   `repiu_exe`와 실행에 무관한 probe만 남기고, Win32·UNIX 전용 실행 타깃과 SDL 데스크톱
   의존 타깃을 제외합니다. **Windows와 Linux 빌드 결과는 바뀌지 않아야 합니다.**
3. `src/platform/web/`를 만들고 13개 플랫폼 헤더 중 wasm에서 성립하는 것을 구현합니다.
   성립하지 않는 다섯(`fault_handler`, `guest_cpu_context`, `guest_stack_switch`,
   `virtual_memory`, `host_process`)은 **실패를 반환하는 명시적 stub**으로 둡니다.
   조용히 성공하는 더미를 만들지 않습니다 — 설계 결정 5의 근거를 볼 것.
4. `repiu_core_probe`의 probe 중 실행 엔진에 의존하지 않는 것을 wasm에서 돌립니다.
   Node로 실행하며, Linux Stage 1과 같은 지표를 냅니다 — **컴파일된 소스 수**와
   **미해결 심볼 수**.
5. 측정 결과를 작업 로그에 적습니다. 값이 채워지는지를 해석보다 **먼저** 확인합니다
   (Task 512가 절차로 넣은 항목).
6. Windows Debug에서 `repiu`와 `repiu_core_probe`를 다시 빌드해 회귀가 없음을 확인합니다.
   Linux i386 빌드 트리는 현재 Release이므로 재구성 비용이 있습니다 — 회귀 확인 범위를
   작업 로그에 명시합니다.
7. `ARCHITECTURE.md`와 `README.md`에 웹 빌드 방법과 **현재 지원 범위(게임 실행 불가)**를
   적습니다. 지원 범위를 적지 않으면 "빌드된다"가 "실행된다"로 읽힙니다.

## 완료 조건

wasm32에서 `repiu_exe`가 빌드되고, 실행 무관 probe가 Node에서 통과해야 합니다.
컴파일 소스 수와 미해결 심볼 수가 로그에 기록되어야 합니다. 기존 Windows 빌드에 회귀가
없어야 합니다.

**게임 실행은 이 작업의 완료 조건이 아닙니다.** 성립하지 않는 다섯 헤더가 stub인 채로
남는 것이 정상이며, 그것이 Stage 3의 입력입니다.

## 범위 밖

인터프리터 backend(Stage 3), x86→wasm 번역(Stage 4), 브라우저 호스트(Stage 5).
명령 census(Stage 2)도 별도 작업입니다.

---

# Web (WebAssembly) Build Configuration Work Order (Stage 1)

Design: [20260828-513-web-wasm-execution.md](../design/20260828-513-web-wasm-execution.md)

1. Add `scripts/build_web_wasm.sh`, taking the same arguments as `build_linux_i386.sh`
   (`--config`, `--target`) and building into `build/web_wasm`. When emsdk is absent or `emcmake`
   is not on PATH, **name the install and activate commands and exit**. A missing toolchain must
   not be buried under hundreds of header errors.
2. Add an `if(EMSCRIPTEN)` branch to `CMakeLists.txt`. The Emscripten configuration keeps
   `repiu_exe` and the execution-free probes and excludes the Win32- and UNIX-only executable
   targets along with the SDL desktop dependants. **The Windows and Linux builds must be
   unchanged.**
3. Create `src/platform/web/` implementing the platform headers that hold on wasm. The five that
   do not (`fault_handler`, `guest_cpu_context`, `guest_stack_switch`, `virtual_memory`,
   `host_process`) stay as **explicit stubs that return failure**. No dummy that quietly
   succeeds — see the rationale in the design's Decision 5.
4. Run the probes in `repiu_core_probe` that do not depend on the execution engine, under Node,
   and report the same metrics as Linux Stage 1: **how many sources compiled** and **how many
   symbols stayed unresolved**.
5. Record the measurements in the work log, confirming the values are actually filled **before**
   interpreting them (the step Task 512 added to the procedure).
6. Rebuild `repiu` and `repiu_core_probe` for Windows Debug to confirm no regression. The Linux
   i386 build tree is currently Release, so reconfiguring costs — state the scope of the
   regression check in the work log.
7. Document the web build and its **current scope (the game does not run)** in `ARCHITECTURE.md`
   and `README.md`. Without the scope, "it builds" reads as "it runs".

## Completion criteria

`repiu_exe` builds for wasm32 and the execution-free probes pass under Node. The compiled-source
count and unresolved-symbol count are recorded in the log. The existing Windows build shows no
regression.

**Running the game is not a completion criterion of this work.** The five headers that do not hold
remaining as stubs is the expected state, and it is Stage 3's input.

## Out of scope

The interpreter backend (Stage 3), x86-to-wasm translation (Stage 4), and the browser host
(Stage 5). The instruction census (Stage 2) is also separate work.
