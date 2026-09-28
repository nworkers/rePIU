# Task 759 작업 로그 — 엔진은 실행 모델로, OS 코드는 platform으로

설계: [20260929-759](../design/20260929-759-execution-model-and-platform-moves.md)
작업 지시: [20260929-759](../work-orders/20260929-759-execution-model-and-platform-moves.md)

## 요약

Task 758이 엔진 안에 만든 플랫폼·아키텍처 디렉터리를 없애고, 규칙을 "OS 코드는 플랫폼 계층으로, 엔진의
아키텍처 차이는 실행 모델로"로 바꿨습니다. 단계 1~3과 단계 4의 앞부분을 마쳤습니다. 플랫폼 계층 밖의 분기는
162개에서 102개가 됐고, 엔진·런타임·헤더에 남은 33개 가운데 아키텍처를 묻는 `#if`는 모델 파일과 진단 파일의
`#error` 확인뿐입니다.

**끝나지 않은 것**: 실행 트램펄린에 Win32 블록 11개가 남았습니다(Win32 스레드 프로시저의 SEH, 예외 진입, 종료
복구의 vectored handler). 엔진의 상태를 쥔 코드라 옮기려면 Win32의 예외 진입 방식을 바꿔야 하고, 그것은
코드를 옮기는 것 이상의 변경입니다. 사용자 결정을 받기 위해 남겨 두었습니다. 도구와 probe의 69개는 설계대로
건드리지 않았습니다.

## 과정

| 단계 | 커밋 | 내용 |
|---|---|---|
| 1 | `8671270` | 규칙 문서 갱신. 엔진 안의 `win32/`, `linux/`, `x86/`, `linux/x64/` 제거. 충돌 보고·WSL 드라이버·심볼·추적 덤프·텔레메트리의 호스트 읽기를 플랫폼 계층으로 |
| 2 | `a2117ba` | MSVC 인라인 어셈블리 thunk 5개와 스택 전환을 `src/platform/win32/`로 |
| 3-1 | `37a856b` | 실행 모델을 runtime 계층으로 옮기고 `none` 모델 추가. AOT 디스패치와 코드 캐시가 모델에 묻게 함. thunk 주소와 long mode Glide 게이트 resolver를 모델 파일로 |
| 3-2 | `11ec448` | 실행 트램펄린: x64 블록 742줄을 `execution_trampoline_cache.cpp`로, x86 진입 함수를 `_direct.cpp`로. 나머지 분기는 모델 확인으로. 재개 코드 5곳을 `ResumeHandledBoundary`로 |
| 4-1 | `2259fee` | 트램펄린의 단순한 Win32 호출(언어 예외 보고, 페이지의 호스트 번호, 폴트 보고용 메모리 읽기, 스레드 종료)을 플랫폼 계층으로 |

설계에서 바뀐 것:

* 실행 모델 인터페이스를 엔진(`src/engine/execution/`)이 아니라 runtime 계층
  (`include/repiu/runtime/execution_model.h`)에 두었습니다. 코드 캐시 생성기(`src/runtime/aot_code_cache.cpp`)가
  같은 질문을 하고, runtime은 엔진보다 아래 계층이기 때문입니다.
* 트램펄린의 분기 가운데 x64 전용 타입을 쓰지 않는 것은 파일로 떼지 않고 같은 코드를 모델 확인
  (`if (RunsLongModeCodeCache())`)으로 감쌌습니다. 예외 처리기 안에서 도는 코드라 함수 경계를 새로 만드는 것보다
  바꾸는 양이 적습니다.

## 검증

기준은 커밋 `a7ad129`에서 모았습니다. 다섯 커밋 모두에서 아래를 확인했습니다.

| 구성 | 확인 | 결과 |
|---|---|---|
| Linux x64 Release | 빌드, core probe 결과 줄 352개 | 기준과 같음 |
| Linux x64 Debug | 빌드, core probe 결과 줄 352개 | 기준과 같음 |
| Linux i386 | 빌드, core probe 결과 줄 290개 | 기준과 같음 |
| Win32 x86 Debug | 빌드, core probe 290줄, aot probe 18개 모드 150줄과 종료 코드, glide issue probe | 기준과 같음 |

| 실행(짧게) | 결과 |
|---|---|
| Linux x64 pumpit8 60초(단계 1, 3-1, 3-2, 4-1) | 폴트 0, 2,427~2,560프레임, swap 대기 중 tick 주입 8,014~8,636, D3D12 선택 |
| Linux x64 pumpitpc 60초(단계 3-2) | 폴트 0, 2,968프레임, cli hold 만료 0 |
| Linux i386 pumpit1 30초(단계 3-2) | `REPIU_WSL_D3D12=0`에서 883프레임, 폴트 0. 그 설정 없이는 창을 열지 못했는데 Task 752의 결함이었고 Task 760에서 고쳤습니다 |
| Win32 pumpit8 25초(모든 단계) | 폴트 0. 프레임 수는 114~450으로 실행마다 크게 달랐습니다 |

확인하지 못한 것:

* **Web 빌드**. 이 머신에 도구(emcc)가 없습니다. `none` 모델과 CMake 목록은 읽어서만 확인했습니다.
* Win32 실행의 성능. 프레임 수 편차가 커서 이 작업 전후를 비교할 수 없었습니다.
* Win32 기준선을 모을 때 첫 빌드가 링크 오류(LNK1236)로 실패했고 다시 돌리자 성공했습니다. 원인은 조사하지
  않았습니다.

로그: `build/task759/`(구성별 빌드 로그와 probe 출력, `runs/`에 실행 로그).

## 찾은 것

* `PushX87Float`(`x87_context.cpp`)와 `CaptureSuspendedThreadSnapshot`(`live_telemetry_snapshot.cpp`)은
  호출하는 곳이 없습니다. 지우지 않고 남겼습니다.
* `src/platform/linux/fault_handler.cpp`가 엔진 헤더(`repiu/engine/guest_write_trace.h`)를 include합니다. 이
  작업 이전부터 있던 것이고 고치지 않았습니다.
* Task 752의 WSL 드라이버 선택이 i386 프로세스에서 x86-64 드라이버를 찾았습니다(Task 760에서 수정).

## 사용자 결정 (2026-09-29)

남은 Win32 블록 11개는 **Win32 전용 코드로 엔진에 두고 이 작업을 여기서 마무리**합니다. Win32의 예외 진입을
플랫폼 계층의 콜백 방식으로 바꾸는 일은 디렉터리 정리의 범위를 넘는 동작 변경이므로 별도 작업으로 둡니다.
규칙 문서(`AGENTS.md`, `docs/CODING_STYLE.md`)에 이 예외를 적었습니다.

## 남은 것

| 남은 것 | 개수 | 상태 |
|---|---|---|
| 트램펄린의 Win32 블록 | 11 | 미착수. Win32 스레드 프로시저(SEH), `DispatchGuestException`과 잘못된 예외 포인터 기록, 종료 복구의 redirect guard·추적 handler |
| Win32 선언이 남은 헤더 | 3 | `exception_rescue_win32.h`(2), `fault_handler.h`(1). 위와 함께 움직입니다 |
| 헤더의 선택 지점 | 4 | 규칙의 예외. 그대로 둡니다 |
| 한 호스트 전용 진단 | 6 | 설계대로 건드리지 않습니다 |
| 모델·진단 파일의 `#error` 확인 | 9 | 의도한 것 |
| 도구와 probe | 69 | 설계대로 건드리지 않습니다 |

남은 Win32 블록을 옮기려면 Win32도 플랫폼 계층의 `InstallFaultHandler`(이미 구현돼 있음)로 폴트를 받고,
종료 복구의 handler는 플랫폼 계층이 콜백으로 등록하게 해야 합니다. `fault_handler.h`가 "transitional"이라고
적어 둔 전환을 끝내는 일이며, Win32의 예외 처리 순서가 바뀝니다.

---

# English

# Task 759 work log — the engine by execution model, OS code to the platform layer

Design: [20260929-759](../design/20260929-759-execution-model-and-platform-moves.md)
Work order: [20260929-759](../work-orders/20260929-759-execution-model-and-platform-moves.md)

## Summary

The platform and architecture directories Task 758 made inside the engine are gone, and the rule is now
"OS code in the platform layer, the engine's difference by architecture as an execution model". Phases 1
to 3 and the first part of phase 4 are done. The branches outside the platform layer went from 162 to
102, and of the 33 left in the engine, the runtime and the headers, the only `#if` that asks about the
architecture is the `#error` check of the model and diagnostic files.

**Not finished**: 11 Win32 blocks remain in the execution trampoline (the SEH of the Win32 thread
procedure, the exception entry, the shutdown recovery's vectored handlers). They hold engine state, and
moving them means changing how Win32 receives faults, which is more than moving code. It is left for the
user's decision. The 69 of the tools and probes are untouched, as designed.

## Steps

| Phase | Commit | What |
|---|---|---|
| 1 | `8671270` | Rule documents. The engine's `win32/`, `linux/`, `x86/` and `linux/x64/` removed. The crash report, the WSL driver, symbols, the trace dump and the telemetry's host reads go to the platform layer |
| 2 | `a2117ba` | The five MSVC inline-assembly thunks and the stack switch go to `src/platform/win32/` |
| 3-1 | `37a856b` | The execution model moves to the runtime layer and gains `none`. The AOT dispatch and the code cache ask the model. Thunk addresses and the long-mode Glide gate resolver go to model files |
| 3-2 | `11ec448` | The execution trampoline: the 742 x64 lines go to `execution_trampoline_cache.cpp`, the x86 entries to `_direct.cpp`. The other branches become tests of the model. Five copies of the resume code become `ResumeHandledBoundary` |
| 4-1 | `2259fee` | The trampoline's plain Win32 calls (the language exception report, a page's host numbers, memory reads for a fault report, stopping a thread) go to the platform layer |

What changed from the design:

* The execution model's interface is in the runtime layer (`include/repiu/runtime/execution_model.h`),
  not in the engine (`src/engine/execution/`): the code cache builder (`src/runtime/aot_code_cache.cpp`)
  asks the same question, and the runtime is a layer below the engine.
* The trampoline's branches that use no x64-only type were not taken out into files; the same code is
  wrapped in a test of the model (`if (RunsLongModeCodeCache())`). The code runs inside fault handlers,
  and that changes less than new function boundaries would.

## Verification

The baseline was collected at commit `a7ad129`. The following was checked at each of the five commits.

| Configuration | Check | Result |
|---|---|---|
| Linux x64 Release | build; 352 core probe result lines | same as the baseline |
| Linux x64 Debug | build; 352 core probe result lines | same as the baseline |
| Linux i386 | build; 290 core probe result lines | same as the baseline |
| Win32 x86 Debug | build; 290 core probe lines; 150 lines and the exit codes of 18 aot probe modes; the glide issue probe | same as the baseline |

| Runs (short) | Result |
|---|---|
| pumpit8 on Linux x64 for 60 s (phases 1, 3-1, 3-2, 4-1) | no faults, 2,427–2,560 frames, 8,014–8,636 ticks injected during the swap wait, D3D12 chosen |
| pumpitpc on Linux x64 for 60 s (phase 3-2) | no faults, 2,968 frames, no cli hold expired |
| pumpit1 on Linux i386 for 30 s (phase 3-2) | 883 frames and no faults with `REPIU_WSL_D3D12=0`. Without it the window did not open, which was a defect of Task 752 and is fixed by Task 760 |
| pumpit8 on Win32 for 25 s (every phase) | no faults. Frame counts varied widely between runs, 114–450 |

Not checked:

* **The web build.** This machine has no tools for it (emcc). The `none` model and the CMake lists were
  checked by reading only.
* Performance on Win32. The frame counts vary too much to compare before and after.
* When the Win32 baseline was collected the first build failed at link (LNK1236) and succeeded when run
  again. The cause was not investigated.

Logs: `build/task759/` (build logs and probe output per configuration, run logs under `runs/`).

## Found

* Nothing calls `PushX87Float` (`x87_context.cpp`) or `CaptureSuspendedThreadSnapshot`
  (`live_telemetry_snapshot.cpp`). They were kept, not removed.
* `src/platform/linux/fault_handler.cpp` includes an engine header (`repiu/engine/guest_write_trace.h`).
  It predates this task and was not changed.
* Task 752's WSL driver choice looked for the x86-64 driver from an i386 process (fixed by Task 760).

## The user's decision (2026-09-29)

The 11 remaining Win32 blocks **stay in the engine as Win32-only code and this task ends here**. Changing
Win32's exception entry to callbacks of the platform layer is a change of behaviour beyond putting
directories in order, so it is left as a task of its own. The rule documents (`AGENTS.md`,
`docs/CODING_STYLE.md`) record the exception.

## What remains

| Remaining | Count | State |
|---|---|---|
| Win32 blocks in the trampoline | 11 | not started. The Win32 thread procedure (SEH), `DispatchGuestException` with the record of malformed exception pointers, the shutdown recovery's redirect guard and trace handler |
| Headers that still declare Win32 | 3 | `exception_rescue_win32.h` (2), `fault_handler.h` (1). They move with the above |
| Selection points in headers | 4 | the rule's exception; they stay |
| Diagnostics of one host | 6 | untouched, as designed |
| `#error` checks of model and diagnostic files | 9 | intended |
| Tools and probes | 69 | untouched, as designed |

Moving the remaining Win32 blocks needs Win32 to receive faults through the platform layer's
`InstallFaultHandler` too (it is already implemented there), and the shutdown recovery's handlers to be
registered by the platform layer as callbacks. That finishes the transition `fault_handler.h` calls
"transitional", and it changes the order in which Win32 handles exceptions.
