# Task 520 — 엔진을 `src/platform/win32/`에서 꺼내기

작업 지시: [20260829-520](../work-orders/20260829-520-engine-out-of-platform-win32.md) ·
작업 로그: [20260829-520](../work-logs/20260829-520-engine-out-of-platform-win32.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md)

## 배경 — 규칙과 실제가 반대입니다

`AGENTS.md`의 구현 규칙은 **"플랫폼 종속 코드는 `src/platform/win32/`, `src/platform/linux/`,
`src/platform/web/` 아래에 둔다"**입니다. 그런데 실제로는 **플랫폼 공용 실행 엔진이
`src/platform/win32/` 안에 살고 Linux 빌드가 그것을 컴파일**하고 있었습니다.

CMake 주석이 그 상태를 스스로 적어 두었습니다 — *"Task 503d-17: no longer inside if(WIN32).
Every one of these compiles on Linux."* 이름과 내용이 어긋난 채로 굳어 있었던 것입니다.

## 확인됨 — 얼마나 어긋나 있었는가

| | 개수 |
|---|---:|
| `src/platform/win32/` 전체 | 119 (82 cpp + 37 h) |
| **Win32 의존이 전혀 없음** | **102** |
| Win32 의존이 남은 것 | 17 |
| 그중 진짜 백엔드(Linux 대응물이 있거나 Windows 전용 추상화) | **8** |
| `include/repiu/platform/win32/` 공개 헤더 | 46 (그중 Win32 타입을 만지는 것은 `x87_context.h` 하나) |

**102 / 119가 플랫폼과 무관한 코드입니다.** 남은 17 중에서도 대부분은 엔진 파일이 `_WIN32`
울타리를 하나 갖고 있을 뿐이고, 그 형태는 이 저장소가 이미 정착시킨 방식입니다.

## 결정 1: `src/engine/`

`AGENTS.md`와 모든 작업 문서가 이 코드를 **"실행 엔진"**이라 부르므로 어휘를 맞춥니다.
`platform/`에는 3a~3d가 세운 추상화 계층(`guest_cpu_context`, `virtual_memory`,
`fault_handler`, `host_thread` …)과 백엔드만 남습니다.

```
src/
  engine/        aot/ boundary/ execution/ telemetry/ cpu_emul/ dos/ bios/ io/ input/ exception/
  platform/      공용 계층 (host_thread, host_time, ...)
    win32/  linux/  web/     백엔드만
include/repiu/
  engine/        46 헤더
  platform/      계층 + win32/ 는 공개 창구 2개
```

## 결정 2: 의존은 한 방향입니다

이동 전 `execution_trampoline.cpp`는 백엔드 헤더 둘을 **인용 include로 백엔드의 소스
디렉터리에서** 보고 있었습니다(`#include "win32_thread_api.h"`). 엔진이 백엔드의 내부를
들여다보는 모양입니다.

그 둘(`win32_thread_api.h`, `exception_rescue_win32.h`)을 `include/repiu/platform/win32/`로
올렸습니다. **엔진은 공개 헤더를 통해서만 백엔드를 봅니다.** 이것이 이번 이동에서 유일하게
기계적이지 않은 변경이고, 경계를 강제하는 자리입니다.

## 결정 3: 두 커밋으로 나눕니다

| 커밋 | 무엇 | 규모 |
|---|---|---:|
| 1 | 파일 이동(`git mv`) + include 경로 | 148 이동, include 219줄 |
| 2 | `repiu::platform::win32` → `repiu::engine` | 164 파일, 약 1,200곳 |

**둘 다 같은 브랜치에서 한 번에 main으로 갑니다** — 중간 상태(엔진 디렉터리에
`platform::win32::` 네임스페이스가 남은 상태)를 main에 남기지 않습니다. 그러면서도 깨지면
어느 쪽인지 즉시 갈립니다.

## 이 작업이 하지 않는 것

* **파일·심볼 이름의 `Win32`/`win32`는 건드리지 않습니다.** `aot_code_cache_win32.cpp`는
  Win32 코드가 하나도 없는데 이름만 그렇고, `ReleaseWin32AotCodeCache` 같은 심볼도
  마찬가지입니다. **세 번째 축이고 별도 작업입니다** — 이번 두 커밋을 순수 경로/네임스페이스
  변경으로 유지해야 검토가 가능합니다.
* **`src/host/win32/main.cpp`는 그대로 둡니다.** 로더 5,577줄이고 같은 종류의 이름 불일치가
  있지만, 이번 범위에 넣으면 커밋 1이 검토 불가능해집니다.
* 동작을 바꾸지 않습니다. 빌드 산출물이 같아야 합니다.
