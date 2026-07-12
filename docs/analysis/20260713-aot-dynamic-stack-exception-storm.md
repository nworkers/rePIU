# AOT Dynamic 모드 스택 페이지 감시 해제(Exception Storm) 분석

## 개요
`aot-dynamic` 모드에서 pumpit1 게임이 실행 중 심각한 예외 발생과 무한 지연 현상(Exception Storm)을 겪었습니다. 디버깅 결과 예외는 주로 게스트 스택 페이지에 접근할 때 발생하였으며, AOT 캐시 코드의 특성과 메모리 감시(Page Watch) 메커니즘의 결함이 결합하여 생긴 문제임을 확인했습니다.

## 현상
- 프로그램 실행 후 몇 초 지나지 않아 프로그램이 멈추는 듯한 Hang 상태에 빠짐.
- 예외 정보를 캡처해보면 `EXCEPTION_ACCESS_VIOLATION (0xc0000005)`와 `EXCEPTION_SINGLE_STEP (0x80000004)` 예외가 같은 메모리 위치에서 무한 반복적으로 발생.
- 예외를 유발한 원본 주소(last_guest_eip)는 `0x30f5014`와 같은 곳이었으며, 이때 프로그램 카운터(EIP)는 AOT 캐시 메모리 대역(예: `0x0f340000`)에 위치함.
- 접근을 시도한 메모리 주소(예: `0x35d6adc`)는 게스트의 스택 메모리 대역이었음.

## 근본 원인 (Root Cause)
1. **스택 메모리 내 실행 코드의 번역:** `PIU.EXE`는 트램폴린(Trampoline)이나 특정 `CALL` 스텁을 통해 스택 메모리 영역에서 일부 코드를 실행하는 동작을 수행합니다.
2. **페이지 쓰기 감시(Write Watch) 적용:** `aot-dynamic` 백엔드는 실행되는 코드를 런타임에 동적으로 캐시(AOT 캐시)로 번역하며, 이때 원본 코드가 변경되는 것을 감지하기 위해 원본 메모리 페이지에 대해 `PAGE_EXECUTE_READ` 보호 속성을 부여하고 감시 목록(`watch_set->watches`)에 등록합니다.
3. **스택 쓰기 명령과 충돌:** AOT 캐시에서 코드가 성공적으로 번역 및 실행되어 스택의 주소를 대상으로 `PUSH`나 `CALL` 등의 일상적인 명령어가 실행될 때, 앞서 스택 페이지가 `PAGE_EXECUTE_READ`로 보호되어 있으므로 `EXCEPTION_ACCESS_VIOLATION`이 발생합니다.
4. **예외 처리의 한계 (Exception Storm):**
   - 위 예외를 Supervisor의 `HandleAotGuestCodeWriteFault`가 가로채어, 페이지 보호를 잠시 해제(`PAGE_EXECUTE_READWRITE`)하고 x86의 Trap Flag (`EFlags |= 0x100`)를 설정하여 단일 명령어 실행을 재개합니다.
   - 단일 명령어가 실행된 직후 `EXCEPTION_SINGLE_STEP` 예외가 트리거되며, 이를 `HandleAotGuestCodeWriteCompletion`가 받아서 성공적으로 쓰기가 완료되었음을 기록(`NoteSuccessfulAotGuestWrite`)합니다.
   - 하지만 쓰기가 완료된 후, 기존 구현에서는 `CompleteWin32AotGuestWrite` 함수가 **페이지가 더 이상 번역 코드를 포함하지 않음에도 불구하고 다시 무조건 `PAGE_EXECUTE_READ` 속성으로 재보호(re-protect)** 하는 로직을 가지고 있었습니다.
   - 그 결과, 다음 스택 관련 명령어(`PUSH` 등)가 나올 때마다 위 과정이 반복되며 수십~수백만 번의 예외와 페이지 권한 변경이 발생해 Exception Storm이 일어났습니다.

## 수정 내용
- 쓰기가 발생하여 기존 AOT 번역 캐시가 무효화된 페이지가, 더 이상 감시할 이유(활성화된 다른 AOT 번역 캐시 블록이 없음)가 없게 된 경우를 검사하는 `ReleaseUnneededWin32AotGuestPageWatches` 함수를 도입.
- 조건에 부합할 경우 `RemoveWin32AotPageWriteWatch` 함수를 호출하여 `watch_set->watches` 목록에서 해당 스택 메모리 페이지를 영구적으로 제거하고 `PAGE_EXECUTE_READWRITE` 상태로 둡니다.
- `HandleAotGuestCodeWriteCompletion`와 `HandleSingleStepTrace`에 반영하여 스택에서 발생하는 Exception Storm의 고리를 끊었습니다.

## 결론
이 버그는 AOT 번역 기반 에뮬레이터에서 흔히 마주하는 "데이터와 코드가 섞인 페이지(혹은 코드가 실행되는 스택)에 대한 동적 캐시 무효화 및 보호 해제 로직의 부재"가 원인이었습니다. 이번 조치로 인해 시스템은 사용되지 않는 번역본에 대해 메모리 보호 페널티를 받지 않으며 안정적으로 `aot-dynamic` 모드 실행이 가능해졌습니다.

---

# AOT Dynamic Mode Stack Page Watch Exception Storm Analysis

## Overview
In `aot-dynamic` mode, the pumpit1 game experienced severe exceptions and infinite stalls (Exception Storm). Debugging revealed that exceptions primarily occurred when accessing guest stack pages. This was confirmed to be caused by a combination of the AOT cache's code characteristics and a flaw in the memory page watch mechanism.

## Symptoms
- The program fell into a seemingly hanging state just seconds after execution.
- Exception captures showed `EXCEPTION_ACCESS_VIOLATION (0xc0000005)` and `EXCEPTION_SINGLE_STEP (0x80000004)` repeating infinitely at the same memory location.
- The original address causing the exception (last_guest_eip) was around `0x30f5014`, and the program counter (EIP) was located in the AOT cache memory band (e.g., `0x0f340000`).
- The accessed memory address (e.g., `0x35d6adc`) was within the guest's stack memory region.

## Root Cause
1. **Translation of Executable Code on Stack:** `PIU.EXE` executes certain code in the stack memory area, via trampolines or specific `CALL` stubs.
2. **Applying Page Write Watch:** The `aot-dynamic` backend dynamically translates running code into an AOT cache at runtime. To detect modifications to the original code, it applies `PAGE_EXECUTE_READ` protection to the original memory page and registers it in `watch_set->watches`.
3. **Conflict with Stack Write Instructions:** When code is successfully translated and executes standard instructions targeting stack addresses, such as `PUSH` or `CALL`, from the AOT cache, an `EXCEPTION_ACCESS_VIOLATION` occurs because the stack page is protected as `PAGE_EXECUTE_READ`.
4. **Limits of Exception Handling (Exception Storm):**
   - The Supervisor's `HandleAotGuestCodeWriteFault` intercepts the exception, temporarily unprotects the page (`PAGE_EXECUTE_READWRITE`), sets the x86 Trap Flag (`EFlags |= 0x100`), and resumes single instruction execution.
   - Immediately after executing the single instruction, an `EXCEPTION_SINGLE_STEP` is triggered. `HandleAotGuestCodeWriteCompletion` catches this to record the successful write via `NoteSuccessfulAotGuestWrite`.
   - However, after the write is completed, the original implementation in `CompleteWin32AotGuestWrite` **unconditionally re-protected the page with `PAGE_EXECUTE_READ` attributes, even if the page no longer contained valid translated code.**
   - Consequently, the next stack-related instruction (e.g., another `PUSH`) would repeat the entire process. This caused millions of exceptions and page permission changes, leading to an Exception Storm.

## Fix Details
- Introduced the `ReleaseUnneededWin32AotGuestPageWatches` function to check if a page, whose AOT translation cache was invalidated by a write, no longer needs watching (i.e., it has no other active AOT cache blocks).
- If the conditions are met, it calls the `RemoveWin32AotPageWriteWatch` function to permanently remove the stack memory page from the `watch_set->watches` list and leaves it in the `PAGE_EXECUTE_READWRITE` state.
- Applied this logic to `HandleAotGuestCodeWriteCompletion` and `HandleSingleStepTrace`, successfully breaking the cycle of the stack Exception Storm.

## Conclusion
This bug was caused by the "lack of dynamic cache invalidation and protection release logic for pages mixed with data and code (or stacks where code is executed)," a common issue in AOT translation-based emulators. With this fix, the system avoids memory protection penalties for unused translations, enabling stable execution in `aot-dynamic` mode.
