# Task 505 작업 지시 — Linux 렌더 백엔드

설계: [20260827-505](../design/20260827-505-linux-render-backend.md) ·
작업 로그: [20260827-505](../work-logs/20260827-505-linux-render-backend.md)

## 1. 울타리를 걷어냅니다

렌더 경로는 **파일 둘**입니다. 처음에는 하나로 보았고, 백엔드만 걷어낸 뒤 실행하니
`opened=1`이 나오면서도 `"Glide dummy fallback activated (no shader)"`로 떨어져 드러났습니다.

| 파일 | 울타리 | 진짜 Win32 API | 스텁 메시지 |
|---|---|---|---|
| `glide_opengl_backend.cpp` | **44** (스텁 33 / Win 전용 11) | **0** | "Win32 OpenGL backend is unavailable" |
| `glide_opengl_shader.cpp` | **13** | **0** | "Win32 GLSL shader backend is unavailable" |

둘 다 `_WIN32` 울타리를 제거하고 Windows 분기를 남깁니다.

* `#if !defined(_WIN32)` … `#else` X `#endif` → **X만 남깁니다**
* `#if defined(_WIN32)` X `#endif` → **X만 남깁니다**

**새 코드를 쓰지 마십시오.** 이 작업은 이식이 아니라 해제입니다. 설계의 결정 1을 보십시오 —
파일은 이미 SDL3로 되어 있고 진짜 Win32 API 호출은 **0개**입니다.

`#if defined(_MSC_VER)` 같은 **컴파일러** 조건은 건드리지 마십시오. 대상은 `_WIN32`뿐입니다.

## 2. `ResolveOpenGlFunction` 한 줄

SDL3의 `SDL_GL_GetProcAddress`는 `SDL_FunctionPointer`(`void(*)()`)를 돌려주는데 지금은
`void*`로 받습니다. MSVC는 허용하고 GCC는 거절합니다.

`void*`로 캐스팅하지 말고 **받는 변수의 타입을 `SDL_FunctionPointer`로 바꾸십시오.** 함수
포인터가 `void*`에 들어간다는 보장은 C++에 없습니다. 실패 판정(0·1·2·3·-1)은
`std::uintptr_t`로의 변환에서 그대로입니다.

## 3. 실패 메시지를 정직하게 고치십시오

`"Win32 OpenGL backend is unavailable"`을 담고 있던 경로가 사라집니다. 남는 실패 메시지 중
호스트를 단정하는 것이 있으면 고치십시오 — **이 파일이 세 번째로 이름에 속은 지점**이고,
설계가 그 이력을 적어 두었습니다.

## 4. 검증 — 컴파일은 완료 조건이 아닙니다

| 대상 | 기준 |
|---|---|
| Linux i386 빌드 | `repiu` 링크까지 성공 |
| **Linux pumpit1 실행** | **창이 열리고 `opened=1`이며 더미 폴백이 아닐 것** |
| Windows Debug 빌드 | 오류 없음 |
| Windows `repiu_core_probe` | `core_probe_total=15 failures=0` |
| Windows `repiu_aot_probe` | exit 0, romset-config 94/0, nvram-path 14/0 |
| Windows pumpit1 | 렌더가 이전과 같음 |

**Windows가 바뀌지 않는다는 것은 주장이 아니라 검증 대상입니다.** 울타리를 걷어도 Windows가
컴파일하는 코드는 글자 하나 달라지지 않아야 하며, 그것을 확인하는 값싼 방법이 있습니다 —
**전처리 결과를 변경 전후로 비교하십시오.** 같으면 Windows 무영향이 증명됩니다.

실행 시 유의:

* **저장소 루트에서 실행하십시오.** 로더가 `roms`와 `build/runtime_mounts`를 상대 경로로 찾습니다.
* `REPIU_EXECUTION_BACKEND=legacy`, `REPIU_EXECUTION_TIMEOUT_MS`로 예산, `REPIU_STALL_TIMEOUT_MS=0`.
* WSLg가 필요합니다. `--headless`로 구성한 트리로는 창이 열리지 않으며, 그 경우 트리를 **버리고**
  다시 구성해야 합니다(SDL이 감지 결과를 캐시합니다).

## 5. 완료 조건

Linux pumpit1이 **창을 엽니다.** `grSstWinOpen`이 `opened=1`을 돌려주고, **그 줄의 메시지가
더미 폴백이 아니어야 합니다.**

> **기준을 좁혔습니다 (실행 중 발견).** 처음에는 `opened=1`만 요구했는데, **더미 폴백도
> `opened=1`을 돌려줍니다** — `dummy_mode_`로 떨어지는 경로가 성공을 반환하기 때문입니다.
> 창이 열리지 않아도 충족되는 기준이었습니다. 앞선 세션이 `exit 0`을 건강함으로 읽은 것과
> 같은 실수이며, 같은 교훈이 다시 적용됩니다 — **성공 신호 하나만 보고 성공을 판정하지 말 것.**
> 배제해야 할 메시지는 `"Glide dummy fallback ..."` 계열 전부입니다.

**무엇이 그려지는지는 이 단계의 완료 조건이 아닙니다.** 창이 열리는 것과 화면이 맞는 것은 다른
질문이고, 둘을 묶으면 어느 쪽이 실패했는지 말할 수 없게 됩니다(503d-20이 같은 이유로 같은
분리를 했습니다). 그림의 정확성은 사람이 보고 판단할 몫이며 별도 과제입니다.

---

# Task 505 Work Order — The Linux render backend

Design: [20260827-505](../design/20260827-505-linux-render-backend.md) ·
Work log: [20260827-505](../work-logs/20260827-505-linux-render-backend.md)

## 1. Take the fences down

The render path is **two files**. It was first read as one, and the second surfaced when unfencing
the backend alone produced `opened=1` that still fell to
`"Glide dummy fallback activated (no shader)"`.

| File | Fences | Real Win32 API | Stub message |
|---|---|---|---|
| `glide_opengl_backend.cpp` | **44** (33 stubs / 11 Windows-only) | **0** | "Win32 OpenGL backend is unavailable" |
| `glide_opengl_shader.cpp` | **13** | **0** | "Win32 GLSL shader backend is unavailable" |

Remove the `_WIN32` fences in both, keeping the Windows branch.

* `#if !defined(_WIN32)` … `#else` X `#endif` → **keep X**
* `#if defined(_WIN32)` X `#endif` → **keep X**

**Write no new code.** This is unfencing, not porting. See the design's decision 1: the file is
already SDL3 and makes **zero** real Win32 API calls.

Leave **compiler** conditions such as `#if defined(_MSC_VER)` alone. The target is `_WIN32` only.

## 2. One line in `ResolveOpenGlFunction`

SDL3's `SDL_GL_GetProcAddress` returns `SDL_FunctionPointer` (`void(*)()`), and the code receives it
into a `void*`. MSVC allows this; GCC refuses.

Do not cast to `void*` — **change the receiving variable's type to `SDL_FunctionPointer`.** C++ does
not guarantee a function pointer fits in a `void*`. The failure test (0, 1, 2, 3, -1) is unchanged,
since it converts to `std::uintptr_t` either way.

## 3. Make the failure messages honest

The path carrying `"Win32 OpenGL backend is unavailable"` disappears. Fix any remaining failure
message that asserts a host — **this file is the third place a name has misled here**, and the design
records that history.

## 4. Verification — compiling is not a completion criterion

| Target | Criterion |
|---|---|
| Linux i386 build | links as far as `repiu` |
| **Linux pumpit1 run** | **a window opens, `opened=1`, and not a dummy fallback** |
| Windows Debug build | no errors |
| Windows `repiu_core_probe` | `core_probe_total=15 failures=0` |
| Windows `repiu_aot_probe` | exit 0, romset-config 94/0, nvram-path 14/0 |
| Windows pumpit1 | renders as before |

**That Windows is unchanged is a claim to verify, not to assert.** Removing the fences must leave the
code Windows compiles character-identical, and there is a cheap way to establish it: **compare the
preprocessed output before and after.** If they match, Windows is proven unaffected.

When running:

* **Run from the repository root.** The loader resolves `roms` and `build/runtime_mounts` relatively.
* `REPIU_EXECUTION_BACKEND=legacy`, a budget via `REPIU_EXECUTION_TIMEOUT_MS`, and
  `REPIU_STALL_TIMEOUT_MS=0`.
* WSLg is required. A tree configured with `--headless` opens no window, and that tree must be
  **discarded** rather than reconfigured (SDL caches what it detected).

## 5. Completion criteria

Linux pumpit1 **opens a window.** `grSstWinOpen` returns `opened=1` **and the message on that line
is not a dummy fallback.**

> **The criterion was tightened during the run.** It first asked only for `opened=1` — but **the dummy
> fallback returns `opened=1` too**, because the path into `dummy_mode_` reports success. That is a
> criterion satisfiable without a window, the same mistake as reading `exit 0` as healthy earlier in
> this work, and it carries the same lesson: **do not judge success from a single success signal.**
> Every `"Glide dummy fallback ..."` message has to be excluded.

**What gets drawn is not a completion criterion here.** A window opening and a correct picture are
different questions, and tying them together makes it impossible to say which one failed — 503d-20
made the same separation for the same reason. Judging the picture needs a person and is separate
work.
