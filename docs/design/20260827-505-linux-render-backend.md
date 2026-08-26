# Task 505 — Linux 렌더 백엔드

작업 지시: [20260827-505](../work-orders/20260827-505-linux-render-backend.md) ·
작업 로그: [20260827-505](../work-logs/20260827-505-linux-render-backend.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260822-503](20260822-503-linux-execution-engine.md)

## 배경

Task 503이 게스트 코드를 Linux에서 실행시켰고, 503d-23이 9초 정지를 없앴습니다. 그런데
**화면은 나오지 않습니다.** 게스트가 `grSstWinOpen`에 도달하면 `opened=0`과
`"Win32 OpenGL backend is unavailable"`을 받습니다.

frontier는 이것을 기록하지 않고 있었습니다(2026-08-27에 추가). 그래서 다음 항목으로
**"렌더 백엔드 이식"**이 잡혔고, 예상 규모는 `glide_opengl_backend.cpp`의 `_WIN32` 울타리
44개였습니다.

## 조사 결과 — 이식할 것이 없습니다

**예상이 빗나갔습니다.** 이 파일은 이미 이식되어 있습니다.

| 물음 | 답 |
|---|---|
| 창과 GL 컨텍스트를 무엇으로 만드나 | `SDL_CreateWindow`·`SDL_GL_CreateContext`·`SDL_GL_MakeCurrent` |
| 이벤트는 | `SDL_PollEvent` |
| 시간은 | `SDL_GetTicksNS`, `std::chrono::steady_clock` |
| GL 함수 해석은 | `SDL_GL_GetProcAddress` |
| **파일 전체의 진짜 Win32 API 호출 수** | **0** |

`wglGetProcAddress`가 한 번 나오지만 **주석 안**입니다 — 실패를 0·1·2·3·-1로 보고하는 그
함수의 관례를 설명하느라 이름을 적은 것이고, 코드는 `SDL_GL_GetProcAddress`를 씁니다.

파일 이름이 `glide_opengl_backend.cpp`이고 위치가 `src/platform/win32/`이며 실패 메시지가
`"Win32 OpenGL backend is unavailable"`이라는 것이 전부 오해를 부릅니다. **이름이 아니라
구현을 봐야 한다**는 규칙은 이 저장소가 이미 두 번 배웠습니다 — 설계 503의 "정정 1"이
`Win32AotPageWriteWatchSet`라는 이름을 보고 `GetWriteWatch`를 쓴다고 단정했다 틀렸고,
frontier 7절이 `cd_audio_wave_out.cpp`를 "이름만 waveOut"이라고 정정했습니다. **세 번째입니다.**

### 그러면 울타리는 무엇인가

503d-10이 이 파일을 **컴파일되게** 하려고 넣은 것입니다. 그 단계의 목표는 컴파일 개수를
올리는 것이었고, 목표는 달성됐습니다. 남은 것이 이것입니다.

| 종류 | 개수 | 내용 |
|---|---|---|
| `#if !defined(_WIN32)` → 즉시 `return false` | **33** | 그 아래 `#else`의 SDL/GL 코드가 통째로 죽어 있음 |
| `#if defined(_WIN32)` 블록 | **11** | 감싼 것도 전부 SDL/GL/std 코드 |

**frontier 8절이 모으는 범주 그대로입니다** — *컴파일되면서 아무것도 안 하는 코드는 컴파일로
볼 수 없다.* 8절은 그런 함수가 넷 남았다고 적고 있고 전부 AOT 경로에 있다고 했습니다.
**여기 한 파일에 33개가 더 있습니다.** 그리고 503d-23이 고친 결함도 같은 계열이었습니다.

### 실제 작업량 — 측정했습니다

측정 가이드가 정한 절차대로, **저장소를 고치지 않고** 울타리를 걷어낸 사본을 Linux i386
툴체인으로 문법 검사했습니다.

```
fences removed: 44
errors: 2
.measure_glide.cpp:404:40: invalid conversion from 'SDL_FunctionPointer'
                            {aka 'void (*)()'} to 'void*' [-fpermissive]
```

**오류 2건이 같은 줄 하나입니다.** `ResolveOpenGlFunction`이 `SDL_GL_GetProcAddress`의
반환값을 `void*`로 받는데, SDL3은 `SDL_FunctionPointer`(`void(*)()`)를 돌려줍니다. MSVC는
암묵 변환을 허용하고 GCC는 허용하지 않습니다.

> 처음 측정은 오류를 4건으로 보고했습니다. 나머지 둘은 `<command-line>`에서 났고, 측정
> 스크립트가 `-DREPIU_VERSION=\"...\"`를 잘못 인용한 것이었습니다. **측정 도구의 결함을
> 대상의 결함으로 세지 않도록** 인용을 고쳐 다시 쟀습니다. 8절이 모으는 함정의 또 다른
> 형태입니다 — 이번에는 은폐가 아니라 **날조**입니다.

## 결정 1: 울타리를 걷어내고, 이식이 아니라 해제로 부릅니다

44개를 전부 제거하고 Windows 분기를 남깁니다. 새 코드를 쓰지 않습니다.

이것을 "렌더 백엔드 이식"이라고 문서에 적지 않습니다. **이식은 503d-10 이전에 이미
끝나 있었고**, 이 작업은 그때 세운 울타리를 걷는 것입니다. 이름을 정확히 두는 이유는
frontier의 "다음에 필요한 것" 목록이 남은 일의 크기를 말해야 하기 때문입니다 — 이 항목이
"백엔드 하나를 새로 쓴다"로 남아 있으면 그 목록 전체의 신뢰도가 떨어집니다.

## 결정 2: 반환 타입은 `SDL_FunctionPointer`로 받습니다

`void*`로 캐스팅해 넘기지 않고, 받는 변수의 타입을 `SDL_FunctionPointer`로 바꿉니다.
그것이 SDL3이 실제로 돌려주는 타입이고, 함수 포인터를 `void*`에 담는 것은 C++이 보장하지
않습니다. 실패 판정(0·1·2·3·-1)은 `std::uintptr_t`로의 변환에서 그대로 유지됩니다.

## 결정 3: 컴파일은 완료 조건이 아닙니다

**이 저장소가 반복해서 배운 것입니다** — 컴파일된다는 것도, 크래시가 없다는 것도 정확한
동작이 아닙니다(Task 229, 503d-23). 그래서 완료 조건은 컴파일이 아니라 **창이 열리고
`grSstWinOpen`이 성공을 돌려주는 것**이며, 그 다음 질문(무엇이 그려지는가)은 사람이 봐야
합니다.

Windows는 **바뀌지 않아야 합니다.** 울타리를 걷어내도 Windows가 컴파일하고 실행하는 코드는
글자 하나 달라지지 않습니다 — `#if defined(_WIN32)`의 참 분기와 `#if !defined(_WIN32)`의
`#else` 분기가 곧 지금 Windows가 쓰는 코드이기 때문입니다. 이것은 주장이 아니라 검증 대상이고,
probe와 실행으로 확인합니다.

## 범위 밖

무엇이 어떻게 그려지는지의 정확성, Wayland, 전체화면, 창 크기 조절 동작, 성능. 이번 작업의
질문은 **"창이 열리는가"** 하나입니다.

---

# Task 505 — The Linux render backend

Work order: [20260827-505](../work-orders/20260827-505-linux-render-backend.md) ·
Work log: [20260827-505](../work-logs/20260827-505-linux-render-backend.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260822-503](20260822-503-linux-execution-engine.md)

## Background

Task 503 got guest code executing on Linux and 503d-23 removed the nine-second stall. **But nothing
is drawn.** A guest reaching `grSstWinOpen` gets `opened=0` and
`"Win32 OpenGL backend is unavailable"`.

The frontier did not record this (added 2026-08-27), so the next item became **"port the render
backend"**, sized at the 44 `_WIN32` fences in `glide_opengl_backend.cpp`.

## What the investigation found — there is nothing to port

**The expectation was wrong.** The file is already portable.

| Question | Answer |
|---|---|
| What creates the window and GL context | `SDL_CreateWindow`, `SDL_GL_CreateContext`, `SDL_GL_MakeCurrent` |
| Events | `SDL_PollEvent` |
| Time | `SDL_GetTicksNS`, `std::chrono::steady_clock` |
| GL function resolution | `SDL_GL_GetProcAddress` |
| **Real Win32 API calls in the whole file** | **zero** |

`wglGetProcAddress` appears once, **inside a comment** — naming it to explain the convention of
reporting failure as 0, 1, 2, 3 or -1. The code calls `SDL_GL_GetProcAddress`.

The file is named `glide_opengl_backend.cpp`, sits in `src/platform/win32/`, and fails with
`"Win32 OpenGL backend is unavailable"`. All three mislead. **Look at the implementation, not the
name** is a rule this repository has already learned twice — design 503's "correction 1" assumed
`Win32AotPageWriteWatchSet` used `GetWriteWatch` from its name and was wrong, and frontier section 7
corrected `cd_audio_wave_out.cpp` as "waveOut in name only". **This is the third.**

### Then what are the fences

503d-10 added them to get the file **compiling**. That sub-stage's goal was the compile count, and it
met it. This is what was left.

| Kind | Count | What it does |
|---|---|---|
| `#if !defined(_WIN32)` → immediate `return false` | **33** | the SDL/GL code in the `#else` below is dead entire |
| `#if defined(_WIN32)` blocks | **11** | what they guard is also SDL/GL/std code |

**Exactly the category frontier section 8 collects** — *code that compiles and does nothing cannot be
seen by compiling.* Section 8 says four such functions remain, all on the AOT path. **Here are 33
more in one file**, and the defect 503d-23 fixed was the same family.

### The real size — measured

Following the measurement guide, a copy with the fences removed was syntax-checked against the Linux
i386 toolchain **without editing the repository**.

```
fences removed: 44
errors: 2
.measure_glide.cpp:404:40: invalid conversion from 'SDL_FunctionPointer'
                            {aka 'void (*)()'} to 'void*' [-fpermissive]
```

**Two errors, and they are one line.** `ResolveOpenGlFunction` takes `SDL_GL_GetProcAddress`'s result
into a `void*`, but SDL3 returns `SDL_FunctionPointer` (`void(*)()`). MSVC allows the implicit
conversion; GCC does not.

> The first measurement reported four errors. The other two came from `<command-line>`: the
> measurement script had mis-quoted `-DREPIU_VERSION=\"...\"`. The quoting was fixed and the
> measurement retaken, so that **a defect in the instrument is not counted as a defect in the
> subject**. Another form of the trap section 8 collects — this time not concealment but
> **fabrication**.

## Decision 1: take the fences out, and call it unfencing rather than porting

Remove all 44 and keep the Windows branch. No new code is written.

The documentation will not call this "porting the render backend". **The port was already done before
503d-10**, and this work takes down the fences that sub-stage put up. The name matters because the
frontier's "what is needed next" list has to convey the size of what remains: leaving this item
reading "write a backend" costs that whole list its credibility.

## Decision 2: receive the return as `SDL_FunctionPointer`

Rather than casting to `void*`, the receiving variable becomes `SDL_FunctionPointer`. That is what
SDL3 actually returns, and C++ does not guarantee that a function pointer fits in a `void*`. The
failure test (0, 1, 2, 3, -1) is unaffected: it converts to `std::uintptr_t` either way.

## Decision 3: compiling is not a completion criterion

**This repository has learned it repeatedly** — neither compiling nor the absence of a crash is
correct behaviour (Task 229, 503d-23). So the criterion is not the build but **a window opening and
`grSstWinOpen` returning success**, and the question after that — what is drawn — needs a person.

Windows must be **unchanged**. Removing the fences changes not one character of what Windows
compiles and runs, because the true branch of `#if defined(_WIN32)` and the `#else` branch of
`#if !defined(_WIN32)` *are* the code Windows uses today. That is a claim to be verified, not
asserted, and the probes and a run are what verify it.

## Out of scope

Whether what is drawn is correct, Wayland, fullscreen, resize behaviour, performance. This task asks
one question: **does a window open.**
