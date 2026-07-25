# 20260725-295 AOT HLE 트랩 주소 변환 작업 로그 / AOT HLE trap address translation work log

## 한국어

### 1. 작업 내용

AOT DBT 모드(`aot-dbt`) 하에서 게스트 코드 컴파일 실행 시 발생하는 하드웨어 보호 예외(특권 명령어 예외 등)의 EIP 주소가 AOT 캐시 내부로 잡히는 현상으로 인해 HLE 예외 핸들러들이 게스트 EIP를 식별하지 못해 호스트가 조기 종료(크래시)되는 문제를 수정하였습니다.

- **`src/platform/win32/execution/execution_trampoline.cpp` 수정**:
  - `AotHleTranslationScope` RAII 도우미 구조체를 새로 정의하였습니다.
  - 이 구조체는 생성 시점에 `win32_context->Eip`가 AOT 캐시 주소 범위에 속해 있는지 확인하고, 속해 있는 경우 `FindAotGuestAddress`를 사용하여 대응하는 게스트 EIP로 임시 치환합니다.
  - 소멸 시점에 HLE 예외 핸들러 체인이 예외 처리에 성공하여 게스트 EIP가 다른 위치로 전진했다면 `FindAotCacheAddress`를 사용하여 해당 게스트 EIP를 새로운 AOT 캐시 주소로 역변환하여 저장합니다. 만약 예외 처리에 실패하여 게스트 EIP가 전진하지 않았다면 원래의 AOT EIP 주소로 원상 복구(롤백)합니다.
  - `DispatchGuestException` 함수 내부의 모든 HLE 핸들러 호출 직전(라인 2659 부근)에 해당 RAII 구조체를 선언하였습니다.

---

### 2. 검증 결과

1. **컴파일 검증**:
   - `cmake --build build/win32_x86_debug --config Debug` 빌드가 성공적으로 완료되었음을 확인했습니다.

2. **AOT 기동 테스트**:
   - 아래 명령어를 통해 AOT 백엔드 및 덤프 활성화 상태로 실행을 시도했습니다:
     `cmd /c "set REPIU_EXECUTION_BACKEND=aot-dbt&&set REPIU_DUMP_LFB_BMP=1&&set REPIU_TIMER_INJECT_LOG=1&&set REPIU_EXECUTION_TIMEOUT_MS=20000&&build\win32_x86_debug\Debug\repiu_loader_win32.exe pumpit1"`
   - 이전 실행 시 기동 2초 만에 첫 `STI` 명령어에서 발생하던 특권 명령어 크래시 현상이 완전히 해결되었으며, 20초 타임아웃 동안 충돌 없이 정상 동작하여 우아하게 타임아웃 종료(`minimal execution attempt timed out`)되었습니다.
   - 통계 로그 상으로도 세그먼트 로드 및 로우 메모리 에뮬레이션 횟수가 수만 건 이상 안정적으로 처리되었음을 검증했습니다:
     * `Win32 handled segment load count: 12224`
     * `Win32 handled segment memory load count: 12964`
     * `Win32 handled low-memory access count: 13078`

3. **LFB 비트맵 덤프 검증**:
   - 실행 종료 후 `build/texture_dumps/` 디렉터리를 확인한 결과, 디버깅 목적의 LFB 프레임 버퍼 덤프 이미지인 `tex_0x1FB_fmt0_640x480_1.bmp` (921,654 bytes) 파일이 정상적으로 생성된 것을 확인했습니다.

---
---

## English

### 1. Work Accomplished

Fixed the premature host termination (crash) during AOT DBT mode (`aot-dbt`) execution, which was caused by hardware protection exceptions (such as `#GP` on privileged instructions) reporting EIPs inside the AOT cache, making them unrecognizable to HLE exception handlers.

- **Modified `src/platform/win32/execution/execution_trampoline.cpp`**:
  - Defined a new RAII helper struct `AotHleTranslationScope`.
  - Upon construction, this struct checks if `win32_context->Eip` is inside the AOT cache range, and if so, temporarily patches `win32_context->Eip` with the resolved guest EIP using `FindAotGuestAddress`.
  - Upon destruction, if an HLE handler successfully resolved the exception and advanced the EIP, the struct reverse-translates the advanced guest EIP back to the AOT cache address using `FindAotCacheAddress`. If unhandled, it rolls back `win32_context->Eip` to the original AOT EIP.
  - Instantiated this RAII scope right before the HLE handler chain invocation (around line 2659) in `DispatchGuestException`.

---

### 2. Verification Results

1. **Compilation Check**:
   - Confirmed that the build via `cmake --build build/win32_x86_debug --config Debug` completed successfully without errors.

2. **AOT Execution Test**:
   - Ran the loader in AOT-DBT mode with dump flags enabled:
     `cmd /c "set REPIU_EXECUTION_BACKEND=aot-dbt&&set REPIU_DUMP_LFB_BMP=1&&set REPIU_TIMER_INJECT_LOG=1&&set REPIU_EXECUTION_TIMEOUT_MS=20000&&build\win32_x86_debug\Debug\repiu_loader_win32.exe pumpit1"`
   - The initial `STI` privileged instruction crash (which occurred within 2s in previous runs) was successfully resolved. The loader executed stably for the full 20s timeout and exited gracefully (`minimal execution attempt timed out`).
   - Verified that tens of thousands of segment loads and low-memory emulation counts were processed correctly:
     * `Win32 handled segment load count: 12224`
     * `Win32 handled segment memory load count: 12964`
     * `Win32 handled low-memory access count: 13078`

3. **LFB Bitmap Dump Verification**:
   - Confirmed that the debug LFB frame buffer dump image `tex_0x1FB_fmt0_640x480_1.bmp` (921,654 bytes) was successfully generated under `build/texture_dumps/`.
