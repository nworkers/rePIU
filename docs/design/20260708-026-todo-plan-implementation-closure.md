# TODO/PLAN 구현 보완 설계

## 배경

이전 작업은 TODO/PLAN 항목을 결과 문서로 정리했지만, 실제 코드 구현은 skipped relocation 상세 목록과 비 Win32 stub에 치우쳐 있었다.
따라서 `docs/TODO.md`에 남긴 다음 후보 작업을 실제 구현 단위로 승격하여 TODO/PLAN 완료 표현을 구현 상태와 맞춘다.

## 목표

* 예외 EIP 주변 바이트를 relocated image에서 덤프할 수 있게 한다.
* guest context 구조체를 추가해 trampoline, SEH trap, HLE dispatcher가 공유할 register/state 표현을 둔다.
* guest stack switching trampoline은 즉시 assembly 전환까지 구현하지 않고, 전환 전 검증 가능한 plan 구조를 만든다.
* HLE dispatcher table 초안을 추가해 INT/DPMI/exception trap 후보를 이름과 종류로 등록할 수 있게 한다.
* selector/descriptor table 최소 모델을 추가해 DPMI descriptor 작업의 공용 기반을 만든다.
* `docs/TODO.md`와 결과 문서를 실제 구현 수준에 맞게 갱신한다.

## 설계 결정

### Exception byte dump

relocated image는 object별 relocated base와 memory buffer를 이미 가진다.
새 runtime helper는 linear address와 window 크기를 입력받아 해당 address를 포함하는 object를 찾고, 앞뒤 byte window를 안전하게 잘라 반환한다.
이 함수는 Win32 실행 여부와 관계없이 analyzer/loader에서 재사용할 수 있다.

### Guest context

`GuestContext`는 일반 레지스터, segment selector, EIP, EFLAGS를 포함한다.
현재 목표는 CPU 에뮬레이터가 아니라 HLE trap 진입 시 host가 원본 x86 상태를 기록하고 수정할 수 있는 최소 상태 표현이다.

### Guest stack switch plan

실제 ESP 전환은 Win32 x86 assembly 또는 compiler-specific intrinsic이 필요하므로 이번 단계에서는 `GuestStackSwitchPlan`을 만든다.
이 plan은 entry, stack top, stack bounds를 검증하고 향후 trampoline 구현의 입력으로 사용한다.

### HLE dispatcher table

`HleDispatcherTable`은 trap kind, vector/opcode, 이름, 구현 상태를 보존한다.
초기 테이블은 DOS INT21, DPMI INT31, privileged instruction exception 후보를 등록한다.
핸들러 구현은 아직 비목표이며, 원본 코드가 trap으로 들어오는 지점을 식별하는 분류 기반을 먼저 둔다.

### Selector table

`SelectorTable`은 selector value, base, limit, flags, present 여부를 보존한다.
초기에는 descriptor 등록/조회만 제공하고 실제 protected-mode 권한 검사는 하지 않는다.

## 검증

* Linux CMake configure/build가 성공한다.
* `repiu_loader_win32`가 exception 발생 시 relocated image byte window를 출력하는 코드 경로를 가진다.
* 새 runtime/HLE 구조는 공용 라이브러리에 포함되어 빌드된다.

# TODO/PLAN Implementation Closure Design

## Background

The previous task summarized TODO/PLAN items in result documents, but the actual code implementation mostly covered skipped relocation details and non-Win32 stubs.
This follow-up promotes the next candidate tasks left in `docs/TODO.md` into implementation units so completion wording matches implementation state.

## Goals

* Dump bytes around an exception EIP from the relocated image.
* Add guest context structures shared by trampoline, SEH trap, and HLE dispatcher paths.
* Represent guest stack switching as a verifiable plan instead of implementing assembly switching immediately.
* Add an HLE dispatcher table draft that can register INT/DPMI/exception trap candidates by name and kind.
* Add a minimal selector/descriptor table model as a shared base for DPMI descriptor work.
* Update `docs/TODO.md` and the result document to match the real implementation state.

## Design Decisions

### Exception byte dump

The relocated image already has per-object relocated bases and memory buffers.
A new runtime helper accepts a linear address and window size, finds the containing object, and returns a safe byte window around that address.
This function can be reused by analyzer/loader paths regardless of Win32 execution support.

### Guest context

`GuestContext` includes general-purpose registers, segment selectors, EIP, and EFLAGS.
The goal is not CPU emulation; it is the minimum state representation that lets the host record and modify original x86 state when an HLE trap is entered.

### Guest stack switch plan

Actual ESP switching requires Win32 x86 assembly or compiler-specific intrinsics, so this step adds `GuestStackSwitchPlan`.
The plan validates entry, stack top, and stack bounds and becomes input for the future trampoline implementation.

### HLE dispatcher table

`HleDispatcherTable` preserves trap kind, vector/opcode, name, and implementation state.
The initial table registers DOS INT21, DPMI INT31, and privileged-instruction exception candidates.
Handlers are still out of scope; this first adds the classification base for original-code trap entry points.

### Selector table

`SelectorTable` preserves selector value, base, limit, flags, and present state.
Initially it only supports descriptor registration and lookup. Full protected-mode permission checks are out of scope.

## Verification

* Linux CMake configure/build succeeds.
* `repiu_loader_win32` has a code path to print relocated image byte windows when an exception occurs.
* The new runtime/HLE structures are included in the shared library and build successfully.
