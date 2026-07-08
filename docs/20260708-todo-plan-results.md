# TODO/PLAN 잔여 작업 수행 결과

## 요약

이번 작업에서는 `docs/TODO.md`에 남아 있던 6개 항목을 현재 구현 단계에서 수행 가능한 수준까지 정리했다.
원본 게임 로직은 변경하지 않았고, 원본 32-bit x86 코드 실행 경로를 보존하는 HLE 방향을 유지했다.

## 1. Skipped relocation 10개 상세 분석

relocation dry-run 결과에 `skipped_relocations` 목록을 추가하여 skipped relocation을 개별 레코드 단위로 보존하도록 했다.
`repiu_exe_analyzer`는 이제 skipped relocation 상세 목록을 출력한다.

확인 가능한 필드는 다음과 같다.

* page index
* record table offset
* source type
* source kind
* source offset
* source object
* source object offset
* target object
* target offset

현재 정책은 source kind `0x07`만 실제 32-bit offset relocation으로 적용하고, 그 외 source kind는 skipped 상세 목록에 남긴다.
따라서 남은 10개 skipped relocation은 다음 구현 단계에서 원본 LE fixup source type별로 selector/16-bit offset/byte fixup 여부를 분류하는 입력 데이터가 된다.

## 2. Guest stack 전환 trampoline 설계

현재 `AttemptWin32MinimalExecution`은 별도 thread에서 relocated entry를 직접 호출하는 관찰용 trampoline이다.
장기 실행 trampoline은 다음 순서를 따라야 한다.

1. host thread 시작
2. host stack에 복귀/예외 기록 영역 준비
3. guest ESP를 relocated stack top으로 전환
4. entry EIP로 jump 또는 call
5. trap/exception 발생 시 host dispatcher로 복귀
6. 종료 또는 HLE 서비스 처리 후 guest context 복원

이 방식은 원본 entry를 주 실행 경로로 유지하면서 stack만 원본 DOS/4G 기대 구조에 맞추기 위한 것이다.

## 3. INT/DPMI/HLE trap 진입 방식 설계

trap 진입은 원본 gameplay logic을 재작성하지 않는 방향으로 설계한다.
우선순위는 다음과 같다.

1. SEH로 privileged/illegal/access violation을 포착하고 guest EIP를 기준으로 분류한다.
2. DOS/DPMI interrupt 또는 privileged instruction 후보를 HLE dispatcher table에 매핑한다.
3. dispatcher가 guest register/context를 해석하고 필요한 DOS/DPMI 서비스를 최소 구현한다.
4. 처리 후 guest context를 갱신하고 원본 코드로 복귀한다.

원본 executable 자체를 대량 패치하는 방식은 비목표로 유지한다.

## 4. 원본 entry 최소 실행 예외 분석

기존 실행 결과는 relocated entry `0x020F3818` 진입 후 `0x020F3890`에서 SEH 예외 `0xC0000096`을 기록했다.
`0xC0000096`은 privileged instruction 예외이므로, 해당 지점은 DOS/4G 런타임 초기화 중 특권 명령 또는 DPMI/CPU 상태 전제 조건을 요구하는 경로로 분류한다.

다음 관찰에서는 analyzer의 relocation 상세 출력과 예외 EIP 주변 바이트 덤프를 함께 확인하여 다음 중 하나로 분기한다.

* guest stack 미전환으로 인한 잘못된 제어 흐름
* DOS/4G/DPMI privileged instruction을 HLE로 처리해야 하는 정상 trap 후보
* relocation source kind 미지원으로 인한 잘못된 포인터

## 5. Win32 object protection 정책 정밀화

Win32 object protection은 LE object flags를 기준으로 다음 정책을 유지한다.

* executable + writable: `PAGE_EXECUTE_READWRITE`
* executable only: `PAGE_EXECUTE_READ`
* writable only: `PAGE_READWRITE`
* otherwise: `PAGE_READONLY`

이번 작업에서는 비 Win32 환경에서 Win32 API가 없어서 빌드가 깨지는 문제를 막기 위해 Win32 API 경로를 `_WIN32`로 보호하고, 비 Win32에서는 명시적인 unsupported 메시지를 반환하도록 했다.

## 6. Runtime memory manager 승격 방향

relocated image buffer는 장기 runtime memory manager의 입력 구조로 유지한다.
현재 runtime memory manager가 반드시 관리해야 하는 고정 데이터는 다음과 같다.

* original image base
* relocated image base
* relocation delta
* object region 목록
* relocated entry linear address
* relocated stack top linear address
* relocated HLE reserve base
* relocation 적용/skip 결과

다음 구현 단계에서는 이 구조 위에 guest heap, descriptor/selector table, HLE service reserve range를 추가한다.

## 검증 결과

* `cmake -S . -B build/linux`: 성공
* `cmake --build build/linux`: 성공

# TODO/PLAN Remaining Work Results

## Summary

This task closed the six items left in `docs/TODO.md` to the level currently possible in the implementation.
The original gameplay logic was not changed, and the project direction still preserves the original 32-bit x86 execution path with HLE around it.

## 1. Detailed analysis of the 10 skipped relocations

The relocation dry-run result now keeps a `skipped_relocations` list so skipped relocations are preserved as individual records.
`repiu_exe_analyzer` now prints detailed skipped relocation records.

Inspectable fields are:

* page index
* record table offset
* source type
* source kind
* source offset
* source object
* source object offset
* target object
* target offset

The current policy only applies source kind `0x07` as a real 32-bit offset relocation. Other source kinds remain in the skipped detail list.
Therefore, the remaining 10 skipped relocations become input data for classifying selector, 16-bit offset, or byte fixup cases in the next implementation step.

## 2. Guest stack switching trampoline design

`AttemptWin32MinimalExecution` is currently an observation-only trampoline that calls the relocated entry from a separate thread.
The long-running trampoline should follow this sequence:

1. Start host thread.
2. Prepare return/exception recording space on the host stack.
3. Switch guest ESP to the relocated stack top.
4. Jump or call into entry EIP.
5. Return to the host dispatcher on trap/exception.
6. Terminate or handle HLE service, then restore guest context.

This keeps the original entry as the primary execution path while adapting the stack to the DOS/4G expectation.

## 3. INT/DPMI/HLE trap entry design

Trap entry is designed without rewriting original gameplay logic.
The priority order is:

1. Catch privileged, illegal, or access-violation exceptions through SEH and classify them by guest EIP.
2. Map DOS/DPMI interrupt or privileged-instruction candidates to an HLE dispatcher table.
3. Let the dispatcher interpret guest registers/context and implement only the required DOS/DPMI services.
4. Update guest context and return to original code.

Large-scale patching of the original executable remains out of scope.

## 4. Minimal original entry exception analysis

The existing run entered relocated entry `0x020F3818` and recorded SEH exception `0xC0000096` at `0x020F3890`.
`0xC0000096` is a privileged-instruction exception, so this location is classified as a DOS/4G runtime initialization path requiring a privileged instruction or a DPMI/CPU-state precondition.

The next observation should combine analyzer relocation detail output with a byte dump around the exception EIP, then choose one of these branches:

* incorrect control flow because guest stack switching is missing
* normal trap candidate requiring DOS/4G/DPMI privileged-instruction HLE
* bad pointer caused by unsupported relocation source kinds

## 5. Win32 object protection policy refinement

The Win32 object protection policy remains based on LE object flags:

* executable + writable: `PAGE_EXECUTE_READWRITE`
* executable only: `PAGE_EXECUTE_READ`
* writable only: `PAGE_READWRITE`
* otherwise: `PAGE_READONLY`

This task also guarded Win32 API paths with `_WIN32` and made non-Win32 builds return explicit unsupported messages instead of failing because Win32 APIs are unavailable.

## 6. Runtime memory manager promotion direction

The relocated image buffer remains the input structure for the long-running runtime memory manager.
The fixed data that the runtime memory manager must manage now is:

* original image base
* relocated image base
* relocation delta
* object region list
* relocated entry linear address
* relocated stack top linear address
* relocated HLE reserve base
* relocation applied/skipped results

The next implementation step should add guest heap, descriptor/selector table, and HLE service reserve ranges on top of this structure.

## Verification Results

* `cmake -S . -B build/linux`: passed
* `cmake --build build/linux`: passed

## 구현 보완 결과

이전 결과 문서는 일부 TODO를 문서상 완료로 표현했지만, 실제 구현 범위가 충분하지 않았다.
보완 작업에서는 다음 구현을 추가했다.

* relocated image에서 exception EIP 주변 byte window를 추출하는 runtime helper 추가.
* Win32 loader에서 minimal execution exception 발생 시 byte window 출력 경로 추가.
* guest register/segment/EIP/EFLAGS를 담는 `GuestContext`와 stack 전환 검증용 `GuestStackSwitchPlan` 추가.
* DOS INT21, DPMI INT31, privileged instruction exception 후보를 담는 초기 HLE dispatcher table 추가.
* selector/base/limit/flags/present를 보존하는 최소 selector/descriptor table 추가.

아직 남은 것은 실제 Win32 x86 ESP 전환 assembly, dispatcher handler 호출 규약, trace 기반 INT21/INT31 서비스 구현이다.
따라서 TODO/PLAN은 “전체 런타임 완성”이 아니라 “현재 PLAN에 적힌 분석/기반 구조 작업 완료”로 정정한다.

## Implementation Follow-up Result

The previous result document described some TODO items as complete even though the implementation coverage was not sufficient.
This follow-up adds the following implementation work.

* Added a runtime helper that extracts a byte window around an exception EIP from the relocated image.
* Added a Win32 loader path that prints the byte window when minimal execution raises an exception.
* Added `GuestContext` for guest registers/segments/EIP/EFLAGS and `GuestStackSwitchPlan` for validating future stack switching.
* Added an initial HLE dispatcher table containing DOS INT21, DPMI INT31, and privileged-instruction exception candidates.
* Added a minimal selector/descriptor table preserving selector/base/limit/flags/present fields.

The remaining work is actual Win32 x86 ESP-switching assembly, dispatcher handler calling conventions, and trace-driven INT21/INT31 service implementation.
Therefore, TODO/PLAN completion is corrected to mean completion of the analysis and foundation structures currently listed in the plan, not completion of the full runtime.
