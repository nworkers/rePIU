# Glide HLE 초기화 예외 0xe06d7363 분석 및 해결 작업 지시서
# Glide HLE Initialization Exception 0xe06d7363 Analysis and Resolution Work Order

## 1. 목적 (Objective)
Glide HLE 초기화 단계(`_GRSSTWINOPEN@28`)의 오동작 및 C++ 예외 `0xe06d7363` 발생 원인을 진단 계측하고, 가속 하드웨어 환경이 불충분할 때 가짜로 초기화를 수행하여 게임이 메인 렌더링 루프로 문제없이 넘어가게 돕는 폴백 처리기를 구현합니다.

Diagnose and instrument the Glide HLE initialization stage (`_GRSSTWINOPEN@28`) to find the source of the C++ exception `0xe06d7363`. Implement a fallback handler that performs a mock initialization when host acceleration hardware is unavailable, permitting the game logic to transition smoothly into the main rendering loop.

---

## 2. 세부 작업 (Tasks)

1. **임시 진단 코드 삽입 (Temp Diagnostic Code)**:
   - `src/platform/win32/execution_trampoline.cpp` 내 `_GRSSTWINOPEN@28` 처리 분기에서 `context->glide_backend.OpenWindowed` 호출부를 `try-catch` 블록으로 감싸고 실행 시도 결과 및 예외를 `stderr`로 즉시 출력하도록 계측합니다.
2. **동적 가속 실패 폴백(더미 모드) 개발 (Develop Fallback Dummy Mode)**:
   - `OpenWindowed` 실패 시, 프로세스를 종료하는 대신 가상의 드라이버 초기화 성공 상태(`opened = true`)로 조작하고 `win32_context->Eax = 1U`를 강제 반환하는 폴백 매핑 처리를 추가합니다.
3. **결과 검증 (Result Verification)**:
   - `piu_1st` 또는 `pumpit1` 타깃을 다시 실행하여 C++ 예외가 회피되고 다음 단계로 안전하게 진행되는지 텔레메트리(`glide_ordinal` 수치 변화 등)와 예외 캐치 여부로 실증합니다.
