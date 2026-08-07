# Task 441 작업 로그 — 호스트 크래시 자체 보고

작업 지시: [20260807-441](../work-orders/20260807-441-host-crash-report.md)

## 1. 결과 — 첫 실행에서 원인을 지목했습니다

Task 440이 다섯 번의 추측으로도 못 찾은 teardown 크래시를, 설치 후 **첫 크래시 실행**이
이렇게 이름 붙였습니다.

```
[repiu-host-crash] code=0xC0000005 (ACCESS_VIOLATION) address=0x771B0867 thread=13948
[repiu-host-crash] access=read target=0x0EC5CE70
[repiu-host-crash] frame 00 ntdll!RtlWakeAllConditionVariable+0x37
[repiu-host-crash] frame 03 repiu_loader_win32!std::condition_variable::notify_all+0x1C
[repiu-host-crash] frame 04 repiu_loader_win32!GlideOpenGlBackend::Close+0xB3
                             (src/platform/win32/glide_opengl_backend.cpp:2752)
[repiu-host-crash] frame 05 RunWin32ExecutionThread (execution_trampoline.cpp:4475)
```

**근인:** timeout 경로가 게스트 스레드를 `TerminateThread`로 죽이는데, 그 스레드는
동기 게이트마다 `host_command_cv_`에서 **대기 중**입니다. 대기 중 살해된 스레드의 wait
block이 조건 변수 목록에 남고, Task 440이 `Close()`에 넣은 `notify_all()`이 그 목록을
깨우다 폴트합니다. 뮤텍스가 아니라 **조건 변수**였고, 그 `notify_all`은 첫 패치부터
모든 변형에 남아 있어 앞선 실험들이 전부 비껴갔습니다.

## 2. 구현

`SetUnhandledExceptionFilter` 하나(`src/platform/win32/exception/host_crash_report.cpp`).
예외 코드·이름·주소, 접근 위반의 읽기/쓰기와 대상 주소, `StackWalk64` + `SymFromAddr`
기반 스택(심볼 없으면 module+offset), 그리고 소스 줄까지 출력합니다. 예외는 삼키지
않고 보고 후 종료합니다.

## 3. 검증

Task 440 브랜치의 크래시 실행에서 위 출력이 나왔고, 그 지목대로 `notify_all`을 없애자
**8/8 정상**이 됐습니다.

## 4. 회고

디버거가 없는 환경에서 **크래시를 스스로 보고하게 만드는 데 든 비용은 한 번**이고,
그것이 다섯 번의 추측을 대체했습니다. 다음에 호스트 크래시를 만나면 이 출력부터
봅니다.

---

# Task 441 Work Log — the loader reports its own crash

Installed, and the **first crashing run named the cause** that five rounds of guessing in Task 440
had missed: `Close` calling `notify_all` on `host_command_cv_` faulted inside
`RtlWakeAllConditionVariable`, because the timeout path terminates the guest thread while it waits
on that very variable and a thread killed mid-wait leaves its wait block linked into the list. It
was the condition variable, not the mutex, and that `notify_all` had been present in every variant
tested, which is why each earlier experiment missed it.

The implementation is one `SetUnhandledExceptionFilter` printing the exception code and address,
the access kind and target for an access violation, and a `StackWalk64` stack symbolised through
dbghelp with source lines, falling back to module and offset without a PDB. It never swallows the
exception. Removing the `notify_all` it pointed at took Task 440's branch from roughly half of all
runs crashing to **8 of 8 clean**.

**Retrospective:** in an environment with no debugger, making the program report its own crash
cost one round and replaced five. A host crash starts here from now on.
