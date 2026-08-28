# Task 513 작업 로그 — 웹(WebAssembly) 빌드 구성 (Stage 1)

설계: [20260828-513](../design/20260828-513-web-wasm-execution.md) ·
작업 지시: [20260828-513](../work-orders/20260828-513-web-wasm-build.md) ·
플랫폼 계층: [`src/platform/web/README.md`](../../src/platform/web/README.md)

## 결과 — 코어는 **한 줄도 고치지 않고** wasm32로 넘어갑니다

| 측정 | 값 |
|---|---:|
| `repiu_exe` 오브젝트 (wasm32) | **64** = 코어 57 + POSIX 재사용 2 + web 5 |
| 코어 소스 중 **수정한 것** | **0** |
| wasm32에서 제외한 실행 엔진 소스 | 82 |
| `librepiu_exe.a`의 미해결 심볼 | 166개 중 **엔진 자신의 것 0개** |
| `repiu_core_probe` 결과 | **9 / 9 통과**, 6개 제외(이름 출력) |
| 포인터 폭 (`void*` / `long` / `size_t`) | **4 / 4 / 4** — i386과 동일 |
| Windows 회귀 | **15 / 15 통과**, 이전과 동일 |

Task 501이 Linux에서 얻은 것과 같은 결론이고, 이번에는 **호스트가 x86이 아닌 곳에서**
얻었습니다. AGENTS.md의 "플랫폼 공용 구조를 우선 설계한다"가 두 번째로 값을 냈습니다.

| 산출물 | 바이트 |
|---|---:|
| `librepiu_exe.a` | 607,036 |
| `repiu_core_probe.wasm` | 587,789 |
| `repiu_core_probe.js` | 64,999 |

의존성도 손대지 않았습니다 — SDL3, libchdr, miniz(lzma 포함), Zydis 모두 Emscripten에서
그대로 configure·빌드됩니다.

**기본 빌드(인자 없음)의 세 타깃이 모두 나옵니다.**

| 타깃 | wasm32 |
|---|---|
| `repiu_core_probe` | 9 / 9 통과 (exit 0) |
| `repiu_glide_issue_probe` | **통과** — Glide 이슈 카탈로그와 기본 핸들러가 wasm에서 그대로 동작합니다 |
| `repiu_chd_cd_probe` | 빌드·링크됨. **실행은 확인하지 못했습니다** — Node에서 Emscripten의 기본 MEMFS는 호스트 파일 시스템을 보지 못하므로 CHD 경로를 넘길 수 없습니다. 인자 없이 부르면 usage로 exit 2입니다 |

`repiu_glide_issue_probe`가 통과한 것은 예상보다 나은 결과입니다. Glide HLE의 카탈로그 경로가
플랫폼 설비를 전혀 쓰지 않는다는 뜻이고, Stage 5에서 WebGL2로 갈 때 그만큼이 이미 서 있습니다.

## 걸린 것 — 값이 채워졌는지 먼저 보라는 절차가 바로 잡았습니다

첫 심볼 측정이 이렇게 나왔습니다.

```
unresolved_total=0
unresolved_repiu=0
```

**해석하면 "미해결 심볼이 하나도 없다"이고, 그건 틀렸습니다.** `llvm-nm`이 PATH에 없어서
`2>/dev/null` 뒤로 조용히 죽었고, 빈 파일을 센 결과가 0이었습니다. emsdk는 `llvm-nm`을
PATH에 올리지 않고 `~/emsdk/upstream/bin/`에만 둡니다.

전체 경로로 다시 재니 실제 값이 나옵니다 — 미해결 213개 중 **166개가 아카이브 밖에서
해결되고, 그중 엔진 자신의 심볼은 0개**입니다. 나머지는 SDL3·Zydis·libc++이고, 링크가
성공한 이유가 그것입니다.

**Task 512가 절차에 넣은 "값이 채워지는지 먼저 확인한다"가 이번에 잡은 것입니다.** 0을
그대로 옮겨 적었다면 이 로그에 "미해결 0"이라는 더 좋아 보이는 거짓이 남았을 것입니다.

## 두 번째로 걸린 것 — 한 타깃이 되는 것을 빌드가 되는 것으로 읽었습니다

`--target repiu_core_probe`가 통과했으므로 웹 빌드가 된다고 적을 뻔했습니다. **인자 없이
`./scripts/build_web_wasm.sh`를 돌리면 실패합니다.**

원인은 `spdlog`입니다. `FetchContent_MakeAvailable`이 spdlog의 **컴파일 라이브러리를 `all`에
넣기 때문에**, wasm 타깃이 하나도 링크하지 않는데도 기본 빌드가 그것을 만듭니다. 그리고
spdlog 1.14.1이 벤더링한 fmt 사본이 Emscripten의 clang을 통과하지 못합니다
(`FMT_COMPILE_STRING`에서 오류 3건).

고친 방법은 우회가 아니라 **범위 결정**입니다 — Emscripten에서는 spdlog를 가져오지 않습니다.
유일한 소비자가 `repiu` 로더이고 그것은 Windows·Linux 타깃입니다.
`spdlog::spdlog_header_only`는 `if(WIN32)`와 Linux 블록 **안에서만** 이름이 불리므로, 없는
타깃은 안 쓰이는 정도가 아니라 **닿을 수 없습니다.**

**이것이 2026-08-27 세션의 세 함정과 같은 모양입니다** — 성공 신호 하나를 성공으로 읽은 것.
`--target`을 준 실행은 그 타깃만 말하고 빌드 전체를 말하지 않습니다. 인자 없는 실행을
한 번 더 돌린 것이 잡았습니다.

## 구현에서 갈린 판단들

### 1. `if(UNIX)`는 Emscripten에서도 참입니다

이것이 이 작업에서 가장 조용한 함정이었습니다. CMake는 Emscripten 툴체인에서 `UNIX`를
켭니다. 저장소의 `if(UNIX)` 다섯 곳은 전부 **"리눅스 호스트"라는 뜻으로** 쓰여 있었고,
그대로 두면 웹 빌드가 x86 어셈블리(`.S` 넷)와 `-m32`, `-no-pie`,
`-Wl,-Ttext-segment=0x40000000`, 그리고 Linux 플랫폼 계층 전체를 끌어옵니다.

다섯 곳 모두 `if(UNIX AND NOT EMSCRIPTEN)`이 됐습니다. 가드가 쓰일 때는 `UNIX`와
"리눅스 호스트"가 같은 것이었고, 지금은 아닙니다.

### 2. 실행 엔진 82개는 제외했고, 이것이 Linux 단계와 다른 유일한 지점입니다

Task 503d-17은 `src/platform/win32`의 소스를 **전부** Linux로 가져갔습니다 — Win32 API가
남은 자리는 파일 안의 울타리 뒤에 있고, Linux도 x86을 실행하기 때문입니다.

웹은 그럴 수 없습니다. 그 82개가 실행 엔진이고, 실행 엔진이 하는 일은 호스트 CPU로
게스트 x86을 돌리는 것입니다. RX 코드 캐시, INT3 sentinel, 트랩 플래그 단일 스텝, 손으로
쓴 thunk — **wasm에 형태가 있는 것이 하나도 없습니다.**

그래서 wasm32 빌드는 `repiu_exe`의 플랫폼 중립 코어 단독이고, 그것이 Stage 1이 증명하려던
바로 그것입니다.

### 3. `host_environment`와 `worker_signal`은 사본을 만들지 않았습니다

둘 다 `src/platform/linux/`의 것을 그대로 씁니다. `#if !defined(_WIN32)`로 열려 있고 내용이
순수 POSIX입니다 — `environ`·`setenv`와 `std::mutex`·`std::condition_variable`뿐입니다.

경로 이름이 `linux`인 것은 정확하지 않지만, Task 503d-17이 `src/host/win32/main.cpp`를
Linux에서 쓰기로 한 것과 같은 판단입니다. **이름의 부정확함보다 사본의 발산이 비쌉니다.**

`safe_memory_copy`는 이 목록에 들어가지 못합니다. Linux 구현이 `process_vm_readv`를 쓰는데
Emscripten에 없고, wasm에서는 애초에 다른 방법이 필요합니다 — 아래 5절.

### 4. stub은 거짓을 반환합니다. `RemoveFaultHandler`도 그렇습니다

성립하지 않는 다섯(`fault_handler`, `guest_cpu_context`, `guest_stack_switch`,
`virtual_memory`, `host_process`)은 전부 실패를 반환하고 이유를 표준 오류로 **한 번** 찍습니다.

`RemoveFaultHandler`가 특히 그렇습니다. 설치된 적이 없으니 참을 돌려줘도 논리적으로는
말이 되지만, 그러면 종료 경로가 **정리를 마쳤다고 기록**하게 됩니다. Task 508의 코어 덤프가
정확히 그런 **올바르게 보이는 순서** 뒤에 숨어 있었습니다.

`ReserveMemory`를 `malloc`으로 흉내내지 않은 것도 같은 이유입니다. 흉내내면 성공합니다 —
쓸 수 있는 메모리가 돌아옵니다. 그런데 이 API의 모든 호출자는 **나중에 보호하려고**
예약합니다. 보호할 수 없는 예약은 호출을 만족시키면서 불변식을 깨고, 그 실패는 여기서 먼
곳에서 드러납니다.

`FlushInstructionCacheRange`만 참입니다. wasm에는 따로 flush할 명령 캐시가 없고 선형
메모리에서 코드가 다시 쓰이지도 않으므로, 참이 **편의가 아니라 정확한 답**입니다.

### 5. `safe_memory_copy`는 wasm에서 성격이 뒤집힙니다

다른 두 호스트에서 이 함수는 **나쁜 읽기에서 복구**합니다 — `ReadProcessMemory`와
`process_vm_readv` 둘 다 첫 번째 못 읽는 페이지에서 멈추고 얼마나 갔는지 알려줍니다.

wasm에는 복구가 없습니다. 선형 메모리 안의 주소는 전부 읽을 수 있어 **경계 안에서는 폴트가
날 수 없고**, 경계 밖은 wasm 트랩입니다 — 시그널이 아니라 모듈의 끝입니다.

그래서 여기서는 범위 검사가 안전망의 **첫 줄이 아니라 전부**입니다. 길이 오버플로를 먼저
보고(감싸는 길이는 통과시켜서는 안 될 비교를 통과합니다), `__builtin_wasm_memory_size`로
현재 한계를 매 호출마다 읽습니다 — `memory.grow`가 실행 중에 한계를 올리기 때문입니다.

### 6. probe가 9개만 돌되, 빠진 여섯의 **이름**을 찍습니다

`core_probe_total=9`만 찍히면 완주로 읽힙니다. 그래서 wasm 실행은 이렇게 끝냅니다.

```
core_probe_total=9
core_probe_failures=0
core_probe_all=true
core_probe_skipped=6 guest_cpu_context virtual_memory fault_handler stack_bridge guest_stack_switch host_thread
core_probe_host=wasm32 (Task 513 Stage 1: the execution engine is not built here)
```

여섯이 빠진 이유는 두 종류이고 둘 다 진짜입니다. `stack_bridge`와 `guest_stack_switch`는
인라인 x86 어셈블리라 컴파일에 닿지도 못합니다. 나머지 넷은 wasm에 없는 설비를 부르므로,
여기서 돌리면 **Stage 1 stub이 거짓을 반환한다는 사실만** 재게 됩니다 — stub이 이미 말하는
내용입니다.

숫자가 아니라 이름을 적은 것은, 다음 사람이 알고 싶은 것이 "몇 개"가 아니라 "어느 여섯"이기
때문입니다.

## 확인하지 않은 것 — 정직하게

**`host_thread.cpp`는 컴파일됐지만 검증되지 않았습니다.** `<sys/syscall.h>`, `<ucontext.h>`,
`pthread_kill`을 쓰는데 Emscripten이 헤더를 전부 제공해서 그대로 빌드됩니다. 그런데
`host_thread` probe는 위 여섯에 들어 있어 **실행되지 않았습니다.** 컴파일되는 것과 동작하는
것은 다르고, 스레드를 실제로 쓰려면 pthreads와 SharedArrayBuffer가 필요합니다 — Stage 3의
항목입니다.

**Windows 회귀는 Debug `repiu_core_probe`만 확인했습니다** (15/15, 이전과 동일). spdlog
변경 뒤 재구성·재빌드해서 **두 번** 확인했습니다. `repiu` 로더와 `repiu_aot_probe`는 다시
빌드하지 않았습니다 — 이번 변경이 닿은 것은 `if(UNIX)` 가드 다섯과 Emscripten 전용 분기,
그리고 probe 드라이버 하나뿐이고, Windows 경로에서는 `NOT EMSCRIPTEN`이 항상 참이라 소스
목록이 그대로입니다.

**Linux i386은 재확인하지 않았습니다.** 빌드 트리가 현재 Release이고 단일 구성 생성기라
Debug로 돌아가려면 SDL 재빌드를 포함한 재구성이 필요합니다. 같은 이유로 변경의 성격상
회귀 위험은 낮습니다 — Linux에서도 `NOT EMSCRIPTEN`이 참입니다. **낮다는 것이 확인했다는
뜻은 아니므로 여기 적습니다.**

## 다음 — Stage 2: 명령 census

Stage 1이 증명한 것은 코어의 이식성이지 실행이 아닙니다. 다음 단위는 **게스트가 실제로 쓰는
x86 명령 형태를 세는 것**이고, 그것이 Stage 3 인터프리터의 크기를 처음으로 숫자로
만듭니다.

지금 저장소가 이름을 붙여 다루는 mnemonic은 **45개뿐**인데, 그것은 폴트 경계에서 **특별
취급이 필요한** 것들입니다. 나머지는 이름 붙여진 적이 없습니다 — 네이티브로 돌기 때문입니다.
census가 답해야 하는 것은 그 나머지입니다.

`repiu_exe_analyzer`가 이미 LE object를 열고(`pumpipx3` 기준 코드/데이터 object가
1,006,108 B), Zydis가 벤더링되어 있으므로 도구는 갖춰져 있습니다.

**x87도 같은 census에서 셉니다.** 게스트는 80비트 확장 정밀도를 쓰는데 wasm에는 `f32`와
`f64`뿐이고, 게임 로직이 그 정밀도에 의존하는지는 **측정된 적이 없습니다.**

## 재현

```bash
git clone --depth 1 https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
source ~/emsdk/emsdk_env.sh

cd <repo> && ./scripts/build_web_wasm.sh --target repiu_core_probe
node build/web_wasm/repiu_core_probe.js
```

심볼 측정은 `~/emsdk/upstream/bin/llvm-nm`을 **전체 경로로** 부릅니다. PATH에 없습니다.

---

# Task 513 Work Log — Web (WebAssembly) Build Configuration (Stage 1)

Design: [20260828-513](../design/20260828-513-web-wasm-execution.md) ·
Work order: [20260828-513](../work-orders/20260828-513-web-wasm-build.md) ·
Platform layer: [`src/platform/web/README.md`](../../src/platform/web/README.md)

## Result — the core reaches wasm32 with **not one line changed**

| Measurement | Value |
|---|---:|
| `repiu_exe` objects (wasm32) | **64** = 57 core + 2 reused POSIX + 5 web |
| Core sources **modified** | **0** |
| Execution-engine sources excluded from wasm32 | 82 |
| Unresolved symbols in `librepiu_exe.a` | 166, of which **0 are the engine's own** |
| `repiu_core_probe` result | **9 / 9 passed**, 6 excluded and named |
| Pointer width (`void*` / `long` / `size_t`) | **4 / 4 / 4** — same as i386 |
| Windows regression | **15 / 15 passed**, unchanged |

The same conclusion Task 501 reached on Linux, this time **on a host that is not x86**. AGENTS.md's
"design shared structures first" has now paid twice.

| Artefact | Bytes |
|---|---:|
| `librepiu_exe.a` | 607,036 |
| `repiu_core_probe.wasm` | 587,789 |
| `repiu_core_probe.js` | 64,999 |

The dependencies needed no work either: SDL3, libchdr, miniz (with lzma), and Zydis all configure
and build under Emscripten as they are.

**All three targets of the default build (no argument) come out.**

| Target | wasm32 |
|---|---|
| `repiu_core_probe` | 9 / 9 passed (exit 0) |
| `repiu_glide_issue_probe` | **passes** — the Glide issue catalogue and its default handler work on wasm unchanged |
| `repiu_chd_cd_probe` | builds and links. **Execution was not confirmed**: Emscripten's default MEMFS under Node cannot see the host filesystem, so no CHD path can be passed. Called with no argument it prints usage and exits 2 |

`repiu_glide_issue_probe` passing is better than expected. It means the Glide HLE's catalogue path
uses no platform facility at all, so that much already stands for the WebGL2 work in Stage 5.

## What caught me — the "check the values are filled" step caught it

The first symbol measurement came out as this:

```
unresolved_total=0
unresolved_repiu=0
```

**Read straight, that says "no unresolved symbols at all", and it is wrong.** `llvm-nm` is not on
PATH, so it died quietly behind `2>/dev/null` and what got counted was an empty file. emsdk leaves
`llvm-nm` in `~/emsdk/upstream/bin/` without putting it on PATH.

Called by full path, the real values appear: of 213 undefined symbols, **166 resolve from outside
the archive and none of them is the engine's own**. The rest are SDL3, Zydis and libc++, which is
why the link succeeded.

**This is what the step Task 512 added to the procedure caught.** Copying the zero forward would
have left a better-looking falsehood in this log.

## What caught me a second time — one target building read as the build building

`--target repiu_core_probe` passed, and this log nearly recorded that the web build works. **Run
`./scripts/build_web_wasm.sh` with no argument and it fails.**

`spdlog` is why. `FetchContent_MakeAvailable` puts spdlog's **compiled library into `all`**, so a
default build builds it even though no wasm target links it — and the copy of fmt vendored by spdlog
1.14.1 does not get past Emscripten's clang (three errors out of `FMT_COMPILE_STRING`).

The fix is a scope decision rather than a workaround: **Emscripten does not fetch spdlog at all.**
Its only consumer is the `repiu` loader, a Windows and Linux target.
`spdlog::spdlog_header_only` is named **only inside** the `if(WIN32)` and Linux blocks, so the
missing target is not merely unused, it is **unreachable**.

**This has the same shape as the 2026-08-27 session's three traps** — one success signal read as
success. A run given `--target` speaks for that target and not for the build. Running it once more
with no argument is what caught it.

## Judgement calls

### 1. `if(UNIX)` is true under Emscripten too

The quietest trap in this work. CMake sets `UNIX` for an Emscripten toolchain. All five `if(UNIX)`
guards in this repository were written to mean **"the Linux host"**, and left alone they would have
pulled x86 assembly (four `.S` files), `-m32`, `-no-pie`,
`-Wl,-Ttext-segment=0x40000000`, and the whole Linux platform layer into the web build.

All five became `if(UNIX AND NOT EMSCRIPTEN)`. When they were written, `UNIX` and "the Linux host"
were the same thing. They are not any more.

### 2. The 82 engine sources are excluded, and this is the only place the web stage differs in kind

Task 503d-17 took **every** source under `src/platform/win32` to Linux — what Win32 API remains sits
behind a fence inside the file that carries it, and Linux still runs x86.

The web cannot do that. Those 82 are the execution engine, and what the execution engine does is run
guest x86 on the host CPU: an RX code cache, INT3 sentinels, trap-flag single-stepping, hand-written
thunks. **Not one of those has a wasm form.**

So the wasm32 build is `repiu_exe`'s platform-neutral core alone, which is exactly what Stage 1 set
out to prove.

### 3. `host_environment` and `worker_signal` did not get copies

Both are used from `src/platform/linux/` unchanged. Each opens with `#if !defined(_WIN32)` and each
is plain POSIX inside — `environ` and `setenv`, a mutex and a condition variable.

The path saying `linux` is inaccurate, but it is the same judgement Task 503d-17 made when it used
`src/host/win32/main.cpp` on Linux: **a divergent copy costs more than an inaccurate name.**

`safe_memory_copy` cannot join them. The Linux implementation calls `process_vm_readv`, which
Emscripten does not have, and wasm needs a different approach regardless — see 5 below.

### 4. The stubs return false, `RemoveFaultHandler` included

All five that do not hold (`fault_handler`, `guest_cpu_context`, `guest_stack_switch`,
`virtual_memory`, `host_process`) return failure and print their reason to standard error **once**.

`RemoveFaultHandler` especially. Nothing was installed, so returning true would be defensible on its
own terms — but it lets a teardown path **record that it finished tidying up**, and Task 508's core
dump hid behind exactly that kind of sequence that looked correct.

`ReserveMemory` is not faked with `malloc` for the same reason. Faking it would succeed: usable
memory comes back. But every caller of that API reserves in order to **protect later**. A
reservation that cannot be protected satisfies the call and breaks the invariant, and that failure
surfaces far from here.

`FlushInstructionCacheRange` is the one that returns true. wasm has no separate instruction cache to
flush and code is never rewritten in linear memory, so true is **the correct answer rather than a
convenient one**.

### 5. `safe_memory_copy` inverts in character on wasm

On the other two hosts this function **recovers from a bad read**: `ReadProcessMemory` and
`process_vm_readv` both stop at the first unreadable page and say how far they got.

wasm has no recovery. Every address inside linear memory is readable, so **an in-bounds read cannot
fault**, and out of bounds is a wasm trap — not a signal, the end of the module.

So here the bounds check is not the first line of the safety net, it is **all of it**. Length
overflow is tested first (a length that wraps would otherwise pass the comparison meant to fail it),
and `__builtin_wasm_memory_size` is read on every call because `memory.grow` raises the limit while
the module runs.

### 6. The probe runs nine, and prints the **names** of the six it did not

`core_probe_total=9` printed alone reads as a complete run. So the wasm run ends like this:

```
core_probe_total=9
core_probe_failures=0
core_probe_all=true
core_probe_skipped=6 guest_cpu_context virtual_memory fault_handler stack_bridge guest_stack_switch host_thread
core_probe_host=wasm32 (Task 513 Stage 1: the execution engine is not built here)
```

The six are absent for two reasons and both are real. `stack_bridge` and `guest_stack_switch` are
inline x86 assembly and never reach the compiler. The other four call facilities wasm does not have,
so running them here would only measure **that the Stage 1 stubs return false** — which the stubs
already say.

Names rather than a count, because what the next reader wants is not "how many" but "which six".

## What was not verified — plainly

**`host_thread.cpp` compiled but is unverified.** It uses `<sys/syscall.h>`, `<ucontext.h>` and
`pthread_kill`, and Emscripten supplies every header, so it builds as it is. But the `host_thread`
probe is one of the six, so it **never ran**. Compiling is not working, and using threads for real
needs pthreads and SharedArrayBuffer — a Stage 3 item.

**The Windows regression check covered Debug `repiu_core_probe` only** (15/15, unchanged), run
**twice** — once before and once after reconfiguring for the spdlog change. `repiu`
and `repiu_aot_probe` were not rebuilt: this change touched five `if(UNIX)` guards, an
Emscripten-only branch, and one probe driver, and on the Windows path `NOT EMSCRIPTEN` is always
true, so the source lists are identical.

**Linux i386 was not re-checked.** Its build tree is currently Release under a single-config
generator, so returning to Debug means reconfiguring, SDL rebuild included. The risk is low for the
same reason — `NOT EMSCRIPTEN` is true there too. **Low is not the same as checked, which is why it
is written here.**

## Next — Stage 2: the instruction census

What Stage 1 proved is the core's portability, not execution. The next unit is **counting the x86
instruction forms the guest actually uses**, which turns the size of the Stage 3 interpreter into a
number for the first time.

The repository names only **45 mnemonics** today, and those are the ones needing special handling at
a fault boundary. The rest have never been named, because they run natively. The census has to
answer for the rest.

The tools are in place: `repiu_exe_analyzer` already opens the LE objects (`pumpipx3`'s code/data
object is 1,006,108 B) and Zydis is vendored.

**x87 gets counted in the same census.** The guest uses 80-bit extended precision, wasm has only
`f32` and `f64`, and whether the game logic depends on that precision **has never been measured**.

## Reproducing

```bash
git clone --depth 1 https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
source ~/emsdk/emsdk_env.sh

cd <repo> && ./scripts/build_web_wasm.sh --target repiu_core_probe
node build/web_wasm/repiu_core_probe.js
```

Symbol measurements call `~/emsdk/upstream/bin/llvm-nm` **by full path**. It is not on PATH.
