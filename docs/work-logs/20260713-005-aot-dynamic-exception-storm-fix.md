# 작업 로그: aot-dynamic 예외 폭풍(Exception Storm) 원인 분석 및 수정

## 개요
`aot-dynamic` 모드에서 pumpit1 게임 실행 시 성능이 극도로 저하되고 수초 후 멈추는 현상이 있었습니다. 분석 결과, 게스트 스택 메모리 페이지에 대한 AOT 번역이 수행되면서 해당 페이지가 'AOT Watched Page(읽기 전용)'로 등록되어, 이후 스택에 대한 모든 쓰기(예: `PUSH`, `CALL`, 지역 변수 조작)에서 접근 위반 예외와 단일 단계(Single Step) 예외가 무한 반복되는 "예외 폭풍(Exception Storm)"이 발생하는 것을 확인하고 수정했습니다.

## 원인
1. `aot-dynamic` 모드에서는 실행 시점에 AOT 번역이 이루어집니다.
2. 스택(Stack) 영역의 메모리 페이지에서 코드가 실행될 경우(예: 트램폴린 코드 등), AOT 번역기가 해당 페이지를 번역하고 `watch_set->watches`에 추가합니다.
3. 이로 인해 스택 페이지에 `PAGE_EXECUTE_READ` 보호가 걸립니다.
4. AOT 캐시에서 코드가 실행되면서 스택에 `PUSH`나 `CALL`을 수행하면, 쓰기 접근 위반(`EXCEPTION_ACCESS_VIOLATION`)이 발생합니다.
5. Supervisor는 예외를 처리하기 위해 페이지 보호를 해제하고, 캐시의 명령어를 실행한 뒤 단일 단계 예외(`EXCEPTION_SINGLE_STEP`)를 통해 제어권을 회수합니다.
6. 하지만 `HandleAotGuestCodeWriteCompletion` 함수에서 단일 단계 예외 처리 후, 무조건적으로 페이지를 다시 `PAGE_EXECUTE_READ`로 재보호(re-protect)합니다.
7. 결과적으로 다음 스택 접근 명령어에서 다시 접근 위반 예외가 발생하여, 프로그램 전체 실행 시간의 대부분이 수백만 번의 예외 처리에 소모되는 심각한 성능 저하와 멈춤(Hang)을 유발했습니다.

## 해결 방법
- 활성화된 AOT 번역 내용이 모두 무효화된(invalidated) 페이지에 대해서는, 페이지 감시를 해제(Remove Watch)하여 무한 예외 루프를 끊어야 합니다.
- `include\repiu\platform\win32\aot_page_coherence_win32.h` 및 `src\platform\win32\aot_page_coherence_win32.cpp` 파일에 `RemoveWin32AotPageWriteWatch` 함수를 추가했습니다. 이 함수는 감시 목록에서 페이지를 제거하고 `PAGE_EXECUTE_READWRITE`로 권한을 완전히 복구합니다.
- `src\platform\win32\execution_trampoline.cpp`에 `ReleaseUnneededWin32AotGuestPageWatches` 함수를 작성했습니다. 성공적인 메모리 쓰기로 인해 AOT 번역 캐시가 무효화된 후, 해당 페이지에 더 이상 유효한 AOT 번역본(혹은 보관된 번역본)이 남아있지 않으면 페이지 감시를 영구 해제하도록 구현했습니다.
- `HandleAotGuestCodeWriteCompletion` 및 `HandleSingleStepTrace` 내에 `NoteSuccessfulAotGuestWrite` 호출 직후 `ReleaseUnneededWin32AotGuestPageWatches`를 호출하도록 수정했습니다.

## 결과
빌드 및 테스트 결과 예외 폭풍이 완전히 사라졌으며, `aot-dynamic` 모드에서 정상적으로 게임이 멈추지 않고 실행되는 것을 확인했습니다. 이전 시도에서 의심했던 호스트 메모리 복사(`memcpy` 등) 문제가 아니라, AOT 아키텍처 상의 스택 페이지 감시 해제 누락이 진짜 원인이었습니다.

---

# Work Log: Analysis and Fix of aot-dynamic Exception Storm

## Overview
When running the pumpit1 game in `aot-dynamic` mode, the performance degraded extremely, and the execution hung after a few seconds. The analysis revealed an "exception storm," where an AOT translation of code on the guest stack page caused it to be registered as an "AOT Watched Page" (Read-Only). Consequently, every subsequent write to the stack (e.g., `PUSH`, `CALL`, local variable accesses) triggered infinite cycles of Access Violation and Single Step exceptions. This issue has been identified and fixed.

## Cause
1. In `aot-dynamic` mode, AOT translation occurs at runtime.
2. If code executes on a memory page within the stack region (e.g., trampoline code), the AOT translator translates it and adds the page to `watch_set->watches`.
3. This applies `PAGE_EXECUTE_READ` protection to the stack page.
4. When executing from the AOT cache and attempting to `PUSH` or `CALL`, a write access violation (`EXCEPTION_ACCESS_VIOLATION`) occurs.
5. The Supervisor handles this exception by unprotecting the page, executing the cache instruction, and regaining control via a Single Step exception (`EXCEPTION_SINGLE_STEP`).
6. However, in `HandleAotGuestCodeWriteCompletion`, after handling the Single Step exception, the page was unconditionally re-protected as `PAGE_EXECUTE_READ`.
7. As a result, the very next stack access instruction triggered another Access Violation. This trapped the program in an exception handling loop occurring millions of times, causing severe performance loss and hanging.

## Solution
- Pages whose active AOT translations have been entirely invalidated must have their watches removed to break the infinite exception loop.
- Added the `RemoveWin32AotPageWriteWatch` function to `include\repiu\platform\win32\aot_page_coherence_win32.h` and `src\platform\win32\aot_page_coherence_win32.cpp`. This function removes the page from the watch list and permanently restores the `PAGE_EXECUTE_READWRITE` permission.
- Created the `ReleaseUnneededWin32AotGuestPageWatches` function in `src\platform\win32\execution_trampoline.cpp`. After successful memory writes invalidate the AOT translation cache, this function permanently releases the page watch if no valid (or quarantined) AOT translations remain on that page.
- Modified `HandleAotGuestCodeWriteCompletion` and `HandleSingleStepTrace` to call `ReleaseUnneededWin32AotGuestPageWatches` immediately after `NoteSuccessfulAotGuestWrite`.

## Result
Builds and tests confirmed that the exception storm has been completely eliminated. The game now runs smoothly in `aot-dynamic` mode without hanging. The actual cause was a missing feature to release stack page watches in the AOT architecture, rather than the host memory copy (`memcpy`) issue suspected in previous attempts.
