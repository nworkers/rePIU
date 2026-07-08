# Win32 ESP 전환 trampoline 작업 지시

## 작업 항목

1. Win32 execution trampoline header에 guest stack execution API를 추가한다.
2. 32-bit MSVC 전용 naked assembly helper로 ESP 전환 call 경로를 구현한다.
3. Win32 loader가 `GuestStackSwitchPlan` 기반 execution API를 호출하도록 연결한다.
4. 실행 결과 출력에 guest stack switch 관련 상태를 추가한다.
5. `docs/TODO.md`를 실제 완료/잔여 작업 상태에 맞게 갱신한다.
6. Win32 x86 빌드와 실행, x64 빌드와 실행을 검증한다.
7. 작업 로그를 남긴다.

## 비목표

* DOS/DPMI service handler 구현
* HLE dispatcher 복귀 규약 구현
* selector/descriptor permission check 구현
* 원본 executable 수정

# Win32 ESP-Switching Trampoline Work Order

## Tasks

1. Add a guest stack execution API to the Win32 execution trampoline header.
2. Implement the ESP-switching call path with a 32-bit MSVC-only naked assembly helper.
3. Wire the Win32 loader to call the execution API based on `GuestStackSwitchPlan`.
4. Add guest stack switch state to the execution result output.
5. Update `docs/TODO.md` to match the real completion and remaining work state.
6. Verify Win32 x86 build/run and x64 build/run.
7. Leave a work log.

## Non-Goals

* Implementing DOS/DPMI service handlers.
* Implementing the HLE dispatcher return convention.
* Implementing selector/descriptor permission checks.
* Modifying the original executable.
