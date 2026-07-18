# 동적 널 포인터 AV 복구 프레임워크 구현 계획서
# Implementation Work Order for Dynamic NULL Pointer AV Recovery Framework

## 개요
## Overview
본 문서에서는 Mesa/Glide 하드웨어 감지 검사 중 널 포인터 역참조로 발생하는 Access Violation(`0x0304DBF8` 등)을 처리하기 위해, 예외 발생 시 디코딩된 베이스 레지스터에 동적으로 HLE 더미 디바이스 페이지 주소를 주입하고 명령어를 재시도하는 범용 복구 프레임워크를 Win32 예외 처리기 계층에 구현하는 계획을 수립합니다.

This document plans the implementation of a generic recovery framework within the Win32 exception handler to solve Access Violations (`0x0304DBF8`, etc.) caused by NULL pointer dereferencing during Mesa/Glide hardware detection. It does this by dynamically patching the faulting base register with an HLE dummy page address and retrying the instruction.

---

## 제안된 변경 사항
## Proposed Changes

### 1. HLE 더미 장치 페이지 할당 (Dummy Device Memory Allocation)
* **대상 파일**: [execution_trampoline.cpp](file:///e:/MYWORK/Projects/rePIU/src/platform/win32/execution/execution_trampoline.cpp)
* **변경 내용**: 
  - 호스트 스레드 초기화 또는 로더 구동 시점에 `VirtualAlloc`을 통해 `64KB` 크기의 `HleDummyDeviceSpace` (`PAGE_READWRITE`) 영역을 커밋하고 0으로 초기화.
  - 전역 또는 스레드 로컬 구조체에 이 주소를 캐싱.

* **Target File**: [execution_trampoline.cpp](file:///e:/MYWORK/Projects/rePIU/src/platform/win32/execution/execution_trampoline.cpp)
* **Changes**:
  - Allocate and commit a `64KB` page (`HleDummyDeviceSpace`, `PAGE_READWRITE`) via `VirtualAlloc` during host thread initialization or loader startup, and zero it out.
  - Cache this pointer in a global or thread-local HLE context structure.

### 2. Win32 예외 처리기 내 Zydis 명령어 디코딩 및 레지스터 패칭 (Zydis Decoding & Register Patching)
* **대상 파일**: [execution_trampoline.cpp](file:///e:/MYWORK/Projects/rePIU/src/platform/win32/execution/execution_trampoline.cpp)
* **변경 내용**:
  - 예외 처리기(`VectoredExceptionHandler` 또는 HLE AV Handler)가 `STATUS_ACCESS_VIOLATION`을 감치했을 때:
    1. 예외가 발생한 주소(`ExceptionAddress`)의 x86 기계어 바이트를 Zydis 디코더로 분석.
    2. 피연산자(Operand) 중 메모리 역참조에 사용된 베이스 레지스터(Base Register) 식별 (예: `EAX`, `EBX`, `ECX`, `EDX`, `ESI`, `EDI`, `EBP`).
    3. 해당 레지스터의 값이 `CONTEXT` 구조체 상에서 `0` (NULL) 인지 확인.
    4. `0`이 맞다면 해당 레지스터의 값을 `HleDummyDeviceSpace` 주소로 덮어씀.
    5. 복구 성공 로그(`[HLE-AV-Fix] Patched NULL base register...`) 출력 후 `EXCEPTION_CONTINUE_EXECUTION` 반환.

* **Target File**: [execution_trampoline.cpp](file:///e:/MYWORK/Projects/rePIU/src/platform/win32/execution/execution_trampoline.cpp)
* **Changes**:
  - In the exception filter (`VectoredExceptionHandler` or HLE AV Handler), when `STATUS_ACCESS_VIOLATION` is caught:
    1. Decode x86 machine bytes at the `ExceptionAddress` using the integrated Zydis decoder.
    2. Extract the base register used for memory dereferencing (e.g., `EAX`, `EBX`, `ECX`, `EDX`, `ESI`, `EDI`, `EBP`).
    3. Check if the value of that register in the thread's `CONTEXT` is `0` (NULL).
    4. If so, replace that register's context value with the virtual address of `HleDummyDeviceSpace`.
    5. Log the recovery operation (`[HLE-AV-Fix] Patched NULL base register...`) and return `EXCEPTION_CONTINUE_EXECUTION`.

---

## 검증 계획
## Verification Plan

### 자동 빌드 및 런타임 분석 (Automated Build & Runtime Analysis)
* **명령어**: `cmake --build build --config Debug`
* **런타임 실행**: `cmd /c "set REPIU_EXECUTION_BACKEND=aot-dynamic&& set REPIU_EXECUTION_TIMEOUT_MS=0&& build\win32_x86_debug\Debug\repiu_loader_win32.exe pumpit1"`
* **합격 기준**:
  1. `0x0304DBF8` 예외 발생 시 프로세스가 중단되지 않고 동적 레지스터 복구가 수행되어 정상적으로 `JZ` 분기를 타고 텍스처 초기화 루프를 완주할 것.
  2. 디버그 로그에 `[HLE-AV-Fix]` 헤더를 포함한 패칭 로그가 출력될 것.
