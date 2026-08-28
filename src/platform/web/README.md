# `src/platform/web` — WebAssembly 플랫폼 계층 (Task 513 Stage 1)

설계: [20260828-513](../../../docs/design/20260828-513-web-wasm-execution.md) ·
작업 지시: [20260828-513](../../../docs/work-orders/20260828-513-web-wasm-build.md)

이 디렉터리는 13개 플랫폼 헤더 중 **wasm에서 성립하는 것만** 구현합니다. 성립하지 않는
다섯은 같은 자리에 **실패를 반환하는 stub**으로 둡니다.

| 파일 | 성격 |
|---|---|
| `safe_memory_copy.cpp` | 실제 구현 — 선형 메모리 범위 검사. wasm은 범위 밖 접근이 **복구 불가능한 트랩**이므로 검사가 유일한 안전망입니다 |
| `web_unsupported.h` | stub이 이유를 **한 번만** 찍는 자리 |
| `guest_cpu_context.cpp` | **stub** — 호스트에 x86 레지스터 컨텍스트가 없습니다 |
| `fault_handler.cpp` | **stub** — 하드웨어 폴트를 사용자 핸들러로 전달하는 개념이 없습니다 |
| `virtual_memory.cpp` | **stub** — 선형 메모리에 페이지 보호가 없습니다 |
| `host_process.cpp` | **stub** — 브라우저에 자식 프로세스가 없습니다 |

## 여기 없는 둘 — `host_environment`와 `worker_signal`

이 둘은 웹 사본을 만들지 않고 `src/platform/linux/`의 것을 **그대로 씁니다.** 두 파일 모두
`#if !defined(_WIN32)`로 열려 있고 내용이 POSIX입니다 — `environ`·`setenv`와
`std::mutex`·`std::condition_variable`뿐이며, Linux 고유 호출이 하나도 없습니다.

같은 코드를 두 벌 두면 한쪽만 고쳐지는 날이 옵니다. 경로 이름이 `linux`인 것은 정확하지
않지만, Task 503d-17이 `src/host/win32/main.cpp`를 Linux에서 쓰기로 한 것과 같은 판단입니다
— 이름의 부정확함보다 사본의 발산이 비쌉니다.

`safe_memory_copy`는 이 목록에 들어가지 못합니다. Linux 구현이 `process_vm_readv`를 쓰는데
Emscripten에 없고, 애초에 wasm에서는 다른 방법이 필요합니다.

## stub이 조용히 성공하지 않는 이유

2026-08-27 세션은 **성공 신호 하나를 성공으로 읽어** 세 번 걸렸습니다 — `exit 0`이 정상
완주가 아니었고, `opened=1`이 창이 열린 것이 아니었으며(더미 폴백도 같은 값을 냈습니다),
`dispatch_entry` 폭증이 전진이 아니었습니다.

그래서 여기 stub은 전부 **거짓을 반환하고, 이유를 표준 오류로 한 번 찍습니다.** 성공을
흉내내는 더미를 두면 Stage 3에서 "실행이 되는 것 같은데 아무 일도 일어나지 않는" 상태를
디버깅하게 되고, 그것이 이 저장소가 이미 비싸게 배운 종류의 함정입니다.

`RemoveFaultHandler`가 참이 아니라 거짓을 돌려주는 것도 같은 이유입니다. 설치된 적이 없는
것을 "떼어냈다"고 보고하면 종료 경로가 정리를 마쳤다고 기록하게 되고, Task 508의 코어 덤프가
정확히 그렇게 **올바르게 보이는 순서** 뒤에 숨어 있었습니다.

---

# `src/platform/web` — the WebAssembly platform layer (Task 513 Stage 1)

Design: [20260828-513](../../../docs/design/20260828-513-web-wasm-execution.md) ·
Work order: [20260828-513](../../../docs/work-orders/20260828-513-web-wasm-build.md)

This directory implements **only the platform headers that hold on wasm**. The five that do not
stay here as **stubs that return failure**.

| File | Nature |
|---|---|
| `safe_memory_copy.cpp` | Real — a linear-memory range check. Out-of-bounds access in wasm is an **unrecoverable trap**, so the check is the only safety net |
| `web_unsupported.h` | Where a stub says its reason, **once** |
| `guest_cpu_context.cpp` | **stub** — the host has no x86 register context |
| `fault_handler.cpp` | **stub** — there is no notion of delivering a hardware fault to a user handler |
| `virtual_memory.cpp` | **stub** — linear memory has no page protection |
| `host_process.cpp` | **stub** — a browser has no child processes |

## The two that are not here — `host_environment` and `worker_signal`

Neither gets a web copy; the Emscripten build uses the ones in `src/platform/linux/` **as they
are**. Both open with `#if !defined(_WIN32)` and both are POSIX inside — `environ` and `setenv`,
`std::mutex` and `std::condition_variable`, and not one Linux-specific call.

Two copies of the same code means a day when only one is fixed. The path saying `linux` is
inaccurate, but it is the same judgement Task 503d-17 made when it used `src/host/win32/main.cpp`
on Linux: a divergent copy costs more than an inaccurate name.

`safe_memory_copy` cannot join that list. The Linux implementation uses `process_vm_readv`, which
Emscripten does not have, and wasm needs a different approach regardless.

## Why the stubs do not quietly succeed

The 2026-08-27 session was caught three times by **reading one success signal as success**:
`exit 0` was not a clean finish, `opened=1` was not an opened window (the dummy fallback returned
the same value), and a surge in `dispatch_entry` was not progress.

So every stub here **returns false and prints its reason to standard error once**. A dummy that
imitates success would turn Stage 3 into debugging a state where "execution seems to work but
nothing happens", and that is a trap this repository has already paid for.

`RemoveFaultHandler` returning false rather than true is the same reasoning. Reporting that
something never installed was "removed" lets a teardown path record that it finished tidying up,
and Task 508's core dump hid behind exactly that kind of **sequence that looked correct**.
