# Task 520 작업 로그 — 엔진을 `src/platform/win32/`에서 꺼냈습니다

설계: [20260829-520](../design/20260829-520-engine-out-of-platform-win32.md) ·
작업 지시: [20260829-520](../work-orders/20260829-520-engine-out-of-platform-win32.md)

## 결과

| | 전 | 후 |
|---|---:|---:|
| `src/platform/win32/` | 119 파일 | **6** |
| `src/engine/` | — | **113** |
| `include/repiu/platform/win32/` | 46 헤더 | **1** |
| `include/repiu/engine/` | — | **46** |

남은 여섯은 Linux 대응물이 있는 백엔드입니다 — `fault_handler`·`virtual_memory`·
`worker_signal`·`safe_memory_copy`·`host_environment`·`host_process`. 남은 공개 헤더 하나는
`win32_thread_api.h`이고, **엔진이 백엔드를 보는 유일한 창구**입니다.

네임스페이스도 함께 옮겼습니다 — `repiu::platform::win32` → `repiu::engine`, 206개 파일
1,220곳. 백엔드는 원래 `repiu::platform`이었으므로 영향이 없습니다.

## 분류를 한 번 바로잡았습니다

`exception_rescue_win32`를 처음에 백엔드로 뒀는데 **틀렸습니다.** 그 헤더가 선언하는
`DispatchGuestFault`·`DispatchGuestException`은 **엔진이 정의하고**(`execution_trampoline.cpp`),
`.cpp`는 20줄짜리 VEH 진입 어댑터일 뿐입니다. SEH 울타리를 쓴 엔진 코드이므로 엔진으로
되돌렸습니다.

**이름이 아니라 무엇을 정의하는지로 갈라야 했습니다.** 파일명에 `_win32`가 있다는 것은
근거가 아닙니다 — `aot_code_cache_win32.cpp`는 Win32 코드가 한 줄도 없습니다.

## 의존을 한 방향으로 고정했습니다

이동 전 `execution_trampoline.cpp`가 이렇게 쓰고 있었습니다.

```cpp
#include "win32_thread_api.h"        // 백엔드의 소스 디렉터리를 인용 include로
```

엔진이 백엔드의 **내부 트리**를 들여다보는 모양이고, CMake의 private include path가 그것을
가능하게 하고 있었습니다. `win32_thread_api.h`를 `include/repiu/platform/win32/`로 올리고
호출부를 한정했습니다. 이제 엔진은 **공개 헤더를 통해서만** 백엔드를 봅니다.

## 검증

| 항목 | 결과 |
|---|---|
| Linux i386 Release 빌드 | 성공 |
| Linux DOS/4GW 샘플 `legacy`/`dynamic` | exit 2, focus 0x10, opcode 0x80 — 3d-19 기준선 |
| Windows Debug 빌드 | 성공 |
| Windows Release 빌드 | 성공 |
| Windows Debug probe | **15 / 15**, 실패 0 |
| Linux Release probe | segfault — **기존 실패** |
| wasm | **미검증** |

**Linux Release probe의 segfault는 회귀가 아닙니다.** v0.0.166이 기록한 것과 정확히 같은
지점(`dos_handle_cache_all=true` 뒤, `== pit_timer ==` 헤더 전)에서 죽습니다.

**wasm은 확인하지 못했습니다.** 이 WSL에 Emscripten이 없습니다. 정적으로는 엔진 목록 전체가
`if(NOT EMSCRIPTEN)` 안에 있고 web 백엔드가 엔진 헤더를 하나도 참조하지 않지만, **빌드로
확인한 것이 아닙니다.**

## 걸린 것 — 두 번 다 제 실수입니다

**하나. `git add -A`가 추적되지 않던 사용자 파일 8개를 담았습니다** (`pass1~6.txt`,
`repiu.exe`, `repiu150.exe`). 바이너리가 히스토리에 들어가면 되돌릴 수 없어 커밋을 되감고
다시 담았습니다.

**둘. 그 수습에서 `git add -u`가 새 경로를 놓쳤습니다.** `-u`는 추적 중인 파일만 갱신하므로,
승격한 공개 헤더 둘이 커밋에서 빠졌습니다. 로컬 디스크에는 있어 빌드가 통과했고 **커밋만으로는
빌드되지 않는 상태**였습니다.

그래서 **설계가 정한 2커밋 구성을 포기했습니다.** 2커밋의 값어치는 "깨지면 어느 쪽인지 즉시
갈린다"인데 커밋 1이 이미 깨져 그 값이 없어졌습니다. 하나의 검증된 커밋이 낫습니다.

**교훈**: 대량 이동에서 `git add -A`는 위험하고 `git add -u`는 불충분합니다. 경로가 바뀌는
작업에서는 **의도한 경로를 명시적으로 add하고, 커밋 후 `git show --name-only`로 실제로 담긴
것을 확인**해야 합니다.

## 범위 밖 — 다음에 남깁니다

* **파일·심볼 이름의 `Win32`.** `aot_code_cache_win32.cpp`, `ReleaseWin32AotCodeCache`,
  `Win32GlideOpenGlBackend` … Win32와 무관해진 이름이 많습니다. 세 번째 축입니다.
* **`src/host/win32/main.cpp`** — 로더 5,577줄이 같은 이름 불일치를 갖고 있습니다.
