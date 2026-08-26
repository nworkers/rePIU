# Task 505 작업 로그 — Linux 렌더 백엔드

설계: [20260827-505](../design/20260827-505-linux-render-backend.md) ·
작업 지시: [20260827-505](../work-orders/20260827-505-linux-render-backend.md)

## 결과

Linux에서 **Glide 창이 열립니다.**

```
_GRSSTWINOPEN@28: mode_supported=1 opened=1
                  message=640x480 logical Glide window opened at 2x (1280x960)
```

이전에는 `opened=0`에 `"Win32 OpenGL backend is unavailable"`이었습니다.

| 파일 | 제거한 울타리 | 진짜 Win32 API |
|---|---|---|
| `glide_opengl_backend.cpp` | **44** (스텁 33 / Win 전용 11) | **0** |
| `glide_opengl_shader.cpp` | **13** | **0** |
| 두 파일의 `ResolveOpenGlFunction` | 각 한 줄 | — |

**새 코드는 한 줄도 쓰지 않았습니다.** 이식이 아니라 해제였습니다.

## 이식할 것이 없었습니다

설계가 적은 대로, 두 파일 모두 **이미 SDL3**입니다 — `SDL_CreateWindow`,
`SDL_GL_CreateContext`, `SDL_GL_MakeCurrent`, `SDL_PollEvent`, `SDL_GL_GetProcAddress`.
`wglGetProcAddress`가 한 번 나오지만 **주석 안**이고, 그 함수의 실패 관례(0·1·2·3·-1)를
설명하느라 이름을 적은 것입니다.

파일 이름·디렉터리·실패 메시지 셋 다 Win32를 가리키는데 구현은 아니었습니다. **이 저장소가
이름에 속은 세 번째**이고, 앞의 둘은 설계 503의 "정정 1"(`Win32AotPageWriteWatchSet`)과
frontier 7절(`cd_audio_wave_out.cpp`)입니다.

울타리는 503d-10이 이 파일을 **컴파일되게** 하려고 넣은 것이고, 그 단계의 목표는 컴파일
개수였습니다. 목표는 달성됐고 이것이 남았습니다 — frontier 8절이 세는 *"컴파일되면서 아무것도
안 하는 코드"*가 8절 기준 넷인데, **여기 두 파일에 40개 넘게 더** 있었습니다.

## 실제 작업량 — 사본으로 쟀습니다

측정 가이드대로 저장소를 고치지 않고 울타리를 걷어낸 사본을 문법 검사했습니다.

| 파일 | 울타리 | 오류 | 실체 |
|---|---|---|---|
| `glide_opengl_backend.cpp` | 44 | **2** | 같은 줄 하나 |
| `glide_opengl_shader.cpp` | 13 | **11** | 같은 줄 하나 (리졸버가 11번 인스턴스화) |

둘 다 `SDL_GL_GetProcAddress`의 반환값을 `void*`로 받는 것이었습니다. SDL3은
`SDL_FunctionPointer`(`void(*)()`)를 돌려주고, MSVC는 암묵 변환을 허용하는데 GCC는 거절합니다.
**캐스팅 대신 받는 변수의 타입을 바꿨습니다** — 함수 포인터가 `void*`에 들어간다는 보장이
C++에 없습니다.

### 측정 도구가 스스로 오류를 만들어 냈습니다

첫 측정은 오류를 **4건**으로 보고했습니다. 둘은 `<command-line>`에서 났고, 원인은 측정
스크립트가 `-DREPIU_VERSION=\"...\"`를 잘못 인용한 것이었습니다.

8절이 모으는 함정의 뒤집힌 형태입니다. **거기서는 하나의 막힘이 뒤의 숫자를 가렸고, 여기서는
측정 도구가 대상에 없는 결함을 만들어 냈습니다.** 인용을 고쳐 다시 재고 나서야 2건이
나왔습니다 — 도구의 결함을 대상의 결함으로 세지 않는 것이 측정의 조건입니다.

## Windows 무영향은 증명했습니다

주장하지 않았습니다. "Windows가 이전에 보던 코드"를 원본에서 독립적으로 재구성해 현재 파일과
대조했습니다.

| 파일 | 계산된 Windows 뷰와의 차이 |
|---|---|
| `glide_opengl_backend.cpp` | 주석 재작성 + `SDL_FunctionPointer` 한 줄 |
| `glide_opengl_shader.cpp` | 주석 추가 + `SDL_FunctionPointer` 한 줄 |

**그 외에는 한 줄도 다르지 않습니다.** 울타리 57개가 전부 Linux 스텁 분기와 지시어만
걷어냈습니다.

## 완료 조건을 실행 중에 좁혔습니다

지시서에 처음 적은 기준은 `grSstWinOpen`이 `opened=1`을 돌려주는 것이었습니다. 백엔드만
걷어내고 돌리자 **정확히 그 값이 나왔는데**, 메시지는 이랬습니다.

```
opened=1 message=Glide dummy fallback activated (no shader)
```

**더미 폴백도 성공을 반환합니다.** 창이 열리지 않아도 충족되는 기준을 제가 써 둔 것입니다.
이 작업에서 조금 전 `exit 0`을 건강함으로 읽은 것과 **같은 실수**이고, 교훈도 같습니다 —
**성공 신호 하나로 성공을 판정하지 말 것.** 기준을 "`opened=1` **그리고 더미 폴백이 아닐 것**"
으로 좁혔습니다.

그리고 그 실패가 두 번째 파일을 찾아 주었습니다. 렌더 경로는 파일 하나가 아니라 **둘**이고,
`glide_opengl_shader.cpp`가 같은 결함을 갖고 있었습니다.

## 검증

| 대상 | 결과 |
|---|---|
| Linux i386 빌드 | `repiu` 링크까지 성공 |
| **Linux pumpit1 창** | **`opened=1`, 640x480 논리 창 2배(1280x960), 더미 아님** |
| 깊이 버퍼 | 요청 24비트 / **승인 24비트** — 실제 GL 컨텍스트 |
| Glide 상태 초기화 | 완주, 오류 0건 (아래) |
| Windows Debug 빌드 | 오류 없음 |
| Windows `repiu_core_probe` | `core_probe_total=15 failures=0` |
| Windows `repiu_aot_probe` | exit 0, romset-config 94/0, nvram-path 14/0 |

게스트가 그리기 직전 상태를 전부 세웁니다 — `grSstSelect`, `grColorCombine`,
`grAlphaCombine`, `grAlphaBlendFunction`, `grAlphaTestFunction`, `grDepthBufferFunction`,
`grDepthMask`, `grColorMask`, `grCullMode`, `grClipWindow`, `grFogMode`.

## 그런데 아직 아무것도 그려지지 않습니다

**이 단계의 완료 조건은 충족됐고, 화면은 아직 안 나옵니다.** 둘은 다른 질문이며 지시서가
그렇게 분리해 두었습니다.

`REPIU_GLIDE_PIXEL_DIAG`로 30초와 **240초**를 돌렸고, 둘 다 **버퍼 스왑 0회**입니다. 창은
열려 있고 상태도 세워졌는데 게스트가 첫 프레임에 도달하지 못합니다.

### 첫 가설은 틀렸습니다 — 느린 것이 아니라 갇힌 것입니다

30초 관측만 두고 **"legacy가 느려 아직 첫 프레임에 못 갔다"**고 적었습니다. 240초를 돌려
확인하니 **아닙니다.**

| 시각 | `last_eip` |
|---|---|
| 18초 | 0x010F5F4C |
| 38초 | 0x010EE19A |
| 98초 | 0x010EE1C7 |
| 178초 | 0x010EE18C |
| 198초 | 0x010EE3B2 |

**38초부터 끝까지 `0x010EE18C`–`0x010EE3B2`, 약 550바이트 구간을 벗어나지 않습니다.**
그동안 디스패치는 2,380만 회를 넘겼고 스왑은 **0회**입니다. 진행이 느린 것이 아니라
**전진하지 않습니다.**

느리다는 설명이 매력적이었던 이유는 관측이 그것과도 모순되지 않았기 때문입니다 —
`dispatch_entry`가 폭증하니 "일하고 있다"로 읽힙니다. **일하는 것과 나아가는 것은 다르고,
그 둘을 가르는 것은 카운터가 아니라 EIP의 궤적입니다.**

### 이것은 503d-23의 정지와 다른 종류입니다

| | 503d-23 | 여기 |
|---|---|---|
| 디스패치 | **동결** (트랩하지 않음) | **폭주** (계속 트랩함) |
| 근인 | 경계 없이 네이티브로 풀려남 | 미확정 — 대기 루프 |

Glide 상태 초기화는 완주했고(`grColorCombine`·`grAlphaBlendFunction`·`grDepthBufferFunction`
등 11종) 오류 메시지는 **0건**입니다. 게임이 **그리기 직전에 무언가를 기다리다 받지 못하는**
모양입니다. 로그에 타이머·IRQ 관련 출력이 전혀 없어 무엇을 기다리는지는 **미확정**입니다.

**Task 505의 범위 밖입니다.** 별도 과제로 세웁니다.

## 남은 것

1. **0x010EE1xx 대기 루프** — 240초 동안 벗어나지 못했습니다. 무엇을 기다리는지가 화면이
   나오기까지 남은 하나이며, 새 과제입니다.
2. **무엇이 그려지는가** — 사람이 봐야 합니다. 측정이 닿는 것은 non-black 픽셀까지입니다.
3. **teardown SIGTRAP**이 Glide 호출 요약 출력을 잘라먹습니다(exit 133). 503d-23이 좁혀 둔
   기존 미해결 건이고, 이번 측정에서는 실행 중에 찍히는 `REPIU_GLIDE_PIXEL_DIAG`로 우회했습니다.

---

# Task 505 Work Log — The Linux render backend

Design: [20260827-505](../design/20260827-505-linux-render-backend.md) ·
Work order: [20260827-505](../work-orders/20260827-505-linux-render-backend.md)

## Result

**A Glide window opens on Linux.**

```
_GRSSTWINOPEN@28: mode_supported=1 opened=1
                  message=640x480 logical Glide window opened at 2x (1280x960)
```

It was `opened=0` with `"Win32 OpenGL backend is unavailable"` before.

| File | Fences removed | Real Win32 API |
|---|---|---|
| `glide_opengl_backend.cpp` | **44** (33 stubs / 11 Windows-only) | **0** |
| `glide_opengl_shader.cpp` | **13** | **0** |
| `ResolveOpenGlFunction` in both | one line each | — |

**Not one line of new code was written.** This was unfencing, not porting.

## There was nothing to port

As the design recorded, both files are **already SDL3** — `SDL_CreateWindow`,
`SDL_GL_CreateContext`, `SDL_GL_MakeCurrent`, `SDL_PollEvent`, `SDL_GL_GetProcAddress`.
`wglGetProcAddress` appears once, **inside a comment**, naming the convention by which that function
reports failure (0, 1, 2, 3, -1).

The filenames, the directory and the failure messages all point at Win32; the implementation does
not. **This is the third time a name has misled here**, after design 503's "correction 1"
(`Win32AotPageWriteWatchSet`) and frontier section 7 (`cd_audio_wave_out.cpp`).

The fences came from 503d-10, put up to get these files **compiling**; that sub-stage's goal was the
compile count and it met it. This is what was left — frontier section 8 counts four functions that
*compile and do nothing*, and **these two files held more than forty more.**

## The real size — measured on a copy

Following the measurement guide, fence-stripped copies were syntax-checked without editing the
repository.

| File | Fences | Errors | What they were |
|---|---|---|---|
| `glide_opengl_backend.cpp` | 44 | **2** | one line |
| `glide_opengl_shader.cpp` | 13 | **11** | one line (the resolver instantiated eleven times) |

Both were `SDL_GL_GetProcAddress`'s result taken into a `void*`. SDL3 returns `SDL_FunctionPointer`
(`void(*)()`); MSVC converts implicitly and GCC refuses. **The receiving variable's type changed
rather than adding a cast** — C++ does not guarantee a function pointer fits in an object pointer.

### The instrument invented errors of its own

The first measurement reported **four**. Two came from `<command-line>`, because the measurement
script had mis-quoted `-DREPIU_VERSION=\"...\"`.

An inverted form of the trap section 8 collects. **There, one obstruction hid the numbers behind it;
here, the instrument manufactured a defect the subject did not have.** Only after fixing the quoting
did the real count of two appear — not counting the instrument's faults as the subject's is a
condition of measuring at all.

## Windows being unaffected was proven

Not asserted. The code Windows saw before was reconstructed independently from the original and
compared against the file now.

| File | Difference from the computed Windows view |
|---|---|
| `glide_opengl_backend.cpp` | a rewritten comment + the `SDL_FunctionPointer` line |
| `glide_opengl_shader.cpp` | an added comment + the `SDL_FunctionPointer` line |

**Nothing else differs by a single line.** All 57 fences removed only Linux stub branches and the
directives themselves.

## The completion criterion was tightened mid-run

The work order first asked that `grSstWinOpen` return `opened=1`. Unfencing the backend alone and
running produced **exactly that value** — with this message:

```
opened=1 message=Glide dummy fallback activated (no shader)
```

**The dummy fallback reports success too.** I had written a criterion satisfiable without a window —
**the same mistake** as reading `exit 0` as healthy earlier in this same work, and it carries the
same lesson: **do not judge success from a single success signal.** The criterion became `opened=1`
**and not a dummy fallback**.

That failure is also what found the second file. The render path is not one file but **two**, and
`glide_opengl_shader.cpp` carried the identical defect.

## Verification

| Target | Result |
|---|---|
| Linux i386 build | links as far as `repiu` |
| **Linux pumpit1 window** | **`opened=1`, a 640x480 logical window at 2x (1280x960), not a dummy** |
| Depth buffer | 24 bits requested / **24 granted** — a real GL context |
| Glide state setup | completes, zero errors (below) |
| Windows Debug build | no errors |
| Windows `repiu_core_probe` | `core_probe_total=15 failures=0` |
| Windows `repiu_aot_probe` | exit 0, romset-config 94/0, nvram-path 14/0 |

The guest sets up everything a game sets up immediately before drawing: `grSstSelect`,
`grColorCombine`, `grAlphaCombine`, `grAlphaBlendFunction`, `grAlphaTestFunction`,
`grDepthBufferFunction`, `grDepthMask`, `grColorMask`, `grCullMode`, `grClipWindow`, `grFogMode`.

## But nothing is drawn yet

**This sub-stage's criterion is met and the screen is still blank.** They are different questions and
the work order separated them deliberately.

Thirty seconds under `REPIU_GLIDE_PIXEL_DIAG` show **zero buffer swaps**, and so do **240**. The
window is open and the state is set, but the guest does not reach its first frame.

### The first hypothesis was wrong — it is not slow, it is stuck

On thirty seconds of observation I wrote that **the legacy backend was simply too slow to have
reached a first frame**. Four minutes says **no**.

| Time | `last_eip` |
|---|---|
| 18 s | 0x010F5F4C |
| 38 s | 0x010EE19A |
| 98 s | 0x010EE1C7 |
| 178 s | 0x010EE18C |
| 198 s | 0x010EE3B2 |

**From 38 seconds to the end it never leaves `0x010EE18C`–`0x010EE3B2`, some 550 bytes.** Over that
span dispatches passed 23.8 million and swaps stayed at **zero**. It is not progressing slowly; it is
**not progressing.**

The slowness story was attractive because the observations did not contradict it — a soaring
`dispatch_entry` reads as "working". **Working and advancing are different, and what separates them
is not a counter but the trajectory of the EIP.**

### This is a different kind of stall from 503d-23's

| | 503d-23 | Here |
|---|---|---|
| Dispatches | **frozen** (not trapping) | **soaring** (trapping constantly) |
| Cause | released natively with no bound | unknown — a wait loop |

Glide state initialisation completes (`grColorCombine`, `grAlphaBlendFunction`,
`grDepthBufferFunction` and eight more) and there are **zero** error messages. It looks like the game
waiting, immediately before drawing, for something it never receives. Nothing timer- or IRQ-related
appears in the log, so what it waits on is **unresolved**.

**Outside Task 505's scope.** It becomes separate work.

## Remaining

1. **The 0x010EE1xx wait loop** — not left in 240 seconds. What it waits on is the one thing left
   between here and a picture, and it is new work.
2. **What is drawn** — for a person to judge. Measurement reaches as far as non-black pixels.
3. **The teardown SIGTRAP** truncates the Glide call-trace summary (exit 133). It is the existing
   unresolved item 503d-23 narrowed, and this measurement worked around it with
   `REPIU_GLIDE_PIXEL_DIAG`, which prints during the run.
