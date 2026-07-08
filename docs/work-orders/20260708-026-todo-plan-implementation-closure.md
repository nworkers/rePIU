# TODO/PLAN 구현 보완 작업 지시

## 작업 항목

1. relocated image address byte window helper를 runtime에 추가한다.
2. guest context와 guest stack switch plan 구조를 runtime에 추가한다.
3. selector/descriptor table 최소 모델을 runtime에 추가한다.
4. HLE dispatcher table 초안을 hle 모듈에 추가한다.
5. Win32 loader가 minimal execution exception 발생 시 exception address 주변 byte window를 출력하도록 연결한다.
6. `docs/TODO.md`, 결과 문서, 작업 로그를 실제 구현 상태에 맞게 갱신한다.
7. Linux CMake configure/build를 검증한다.

## 비목표

* 실제 ESP 전환 assembly 구현
* DOS/DPMI handler 전체 구현
* protected-mode 권한 검사 전체 구현
* 원본 executable 패치

# TODO/PLAN Implementation Closure Work Order

## Tasks

1. Add a relocated image address byte-window helper to runtime.
2. Add guest context and guest stack switch plan structures to runtime.
3. Add a minimal selector/descriptor table model to runtime.
4. Add an HLE dispatcher table draft to the hle module.
5. Wire the Win32 loader to print a byte window around the exception address when minimal execution raises an exception.
6. Update `docs/TODO.md`, the result document, and the work log to match the actual implementation state.
7. Verify Linux CMake configure/build.

## Non-Goals

* Actual ESP-switching assembly implementation.
* Full DOS/DPMI handler implementation.
* Full protected-mode permission checks.
* Original executable patching.
