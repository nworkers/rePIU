# TODO/PLAN 잔여 작업 정리 작업 지시

## 목표

`docs/TODO.md`와 기존 PLAN 문서에 남은 후속 작업을 현재 코드/문서 상태에 맞게 모두 정리하고 결과를 Markdown 문서로 저장한다.

## 작업 항목

1. skipped relocation 상세 분석을 위해 dry-run 결과에 skipped relocation 목록을 보존한다.
2. analyzer 출력에 skipped relocation 상세 목록을 추가한다.
3. 비 Win32 환경에서 Win32 전용 API가 빌드를 깨지 않도록 unsupported stub 경로를 둔다.
4. guest stack trampoline, INT/DPMI/HLE trap, privileged instruction 예외, Win32 object protection, runtime memory manager 방향을 결과 문서에 정리한다.
5. `docs/TODO.md`를 완료 상태로 갱신한다.
6. 빌드 검증을 수행하고 작업 로그를 남긴다.

## 검증

* `cmake -S . -B build/linux`
* `cmake --build build/linux`

# TODO/PLAN Remaining Work Closure Work Order

## Goal

Close every follow-up item left in `docs/TODO.md` and existing PLAN documents according to the current code/documentation state, then save the results in a Markdown document.

## Tasks

1. Preserve skipped relocation records in the dry-run result for detailed skipped relocation analysis.
2. Add detailed skipped relocation output to the analyzer.
3. Add unsupported stub paths so Win32-only APIs do not break non-Win32 builds.
4. Summarize the guest stack trampoline, INT/DPMI/HLE trap, privileged-instruction exception, Win32 object protection, and runtime memory manager direction in the result document.
5. Update `docs/TODO.md` to a completed state.
6. Run build verification and leave a work log.

## Verification

* `cmake -S . -B build/linux`
* `cmake --build build/linux`
