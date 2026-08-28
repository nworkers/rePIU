# Task 520 작업 지시 — 엔진 이동

설계: [20260829-520](../design/20260829-520-engine-out-of-platform-win32.md) ·
작업 로그: [20260829-520](../work-logs/20260829-520-engine-out-of-platform-win32.md)

## 1. `git mv`를 쓰십시오

`mv` + `git add`가 아니라 `git mv`입니다. 148개가 한 번에 움직이므로 rename 추적이 끊기면
이 트리의 `git log --follow`가 전부 여기서 멈춥니다.

이동 스크립트를 남기십시오. 목록을 손으로 옮기면 재현도 검토도 안 됩니다.

## 2. 남길 것은 여덟입니다

Linux 대응물이 있는 여섯(`fault_handler`·`virtual_memory`·`worker_signal`·
`safe_memory_copy`·`host_environment`·`host_process`)과 Windows 전용 둘
(`exception_rescue_win32`, `win32_thread_api`).

**셸 목록을 여러 줄로 쓰지 마십시오.** `case " $keep " in *" $base "*)` 형태는 줄바꿈에서
매치가 깨져 파일이 조용히 따라갑니다. 이 작업에서 실제로 `worker_signal_win32.cpp` 하나가
그렇게 넘어갔고 CMake가 잡아 주었습니다.

## 3. include는 세 종류입니다

| 형태 | 어디에 | 건수 |
|---|---|---:|
| `repiu/platform/win32/...` | 전역 | 203 |
| `"../../platform/win32/..."` 상대 | `main.cpp`, probe 7개 | 16 |
| `"foo.h"` 짧은 이름 | 엔진 내부, private include path로 해결 | (경로 목록 갱신) |

**세 번째를 잊기 쉽습니다.** CMake의 `target_include_directories`에 있는 서브시스템 목록을
같이 옮기지 않으면 짧은 이름 include가 전부 깨집니다.

## 4. 검증 — 세 호스트

| 항목 | 기준 |
|---|---|
| Linux Release 빌드 | 성공 |
| Linux DOS/4GW 샘플 `legacy`/`dynamic` | exit 2, focus 0x10, opcode 0x80 |
| Windows Debug·Release 빌드 | 성공 |
| Windows Debug probe | 15 / 15 |
| wasm | Emscripten이 있으면 빌드, 없으면 **미검증이라고 적을 것** |

**Linux Release probe의 segfault는 기존 실패입니다**(v0.0.166). 같은 지점
(`dos_handle_cache_all=true` 뒤, `== pit_timer ==` 전)인지 확인하고, 같으면 회귀가 아닙니다.

## 5. 하지 마십시오

* 파일·심볼 이름의 `Win32`를 건드리지 마십시오. 별도 작업입니다.
* 동작을 바꾸지 마십시오. 이 커밋에서 로직 수정이 하나라도 들어가면 검토가 불가능해집니다.
