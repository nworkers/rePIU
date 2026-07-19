# Task 245 작업 지시: AOT retire 시 inline-cache guard 리셋 구현 및 return fast-path 복원

## 배경

Task 243은 저장 레지스터 epilogue(`add esp,4; pop ebp; pop edi; pop esi; pop ecx; pop ebx; ret`)의
zero-EIP 종료를 return inline-cache 대신 breakpoint dispatcher를 강제하는 가드로 차단했다.
그 결과 고빈도 epilogue 반환마다 예외 경로가 강제되어 `aot-dynamic` 진행도가 약 2.1만에서
정체한다(같은 시간 trap backend는 약 263만).

Task 238 설계 문서([20260719-238-aot-retired-target-inline-cache.md](../design/20260719-238-aot-retired-target-inline-cache.md))는
근본 결함을 이미 확인했다: `RetireWin32AotGuestPage`는 retire 페이지에 속한 address-map
entry의 첫 바이트만 `INT3`로 바꾸고, **다른 번역 블록의 학습된 inline-cache guard는 그대로
남는다.** 학습된 hit은 retired entry로 직접 점프해 매 전송마다 trap을 유발하고, trap 지점이
miss tail이 아니므로 소스 슬롯은 영구히 재패치되지 않는다. 이 설계는 승인되었으나 코드로
구현되지 않았다(2026-07-19 코드 검토로 확인).

## 작업 항목

1. `RetireWin32AotGuestPage`(`src/platform/win32/aot_page_coherence_win32.cpp`)에
   Task 238 설계의 guard 리셋을 구현한다.
   - cache가 writable인 동일 보호 전환 구간에서 모든 inline-cache site를 순회한다.
   - guard가 설치 형태(`0F 85`)이고 target immediate(guest 주소)가 retire 페이지 안이면
     guard를 초기 miss 형태(`E9 rel32 → miss tail` + `90`)로 복원한다.
   - target immediate와 jump displacement는 보존한다(다음 miss에서 dispatcher가 현재
     generation으로 재패치).
   - 리셋 수를 `Win32AotGuestPageRetireResult::guard_reset_count`로 보고하고, 기존
     `[repiu-live-debug]` 샘플링 방식으로 stderr에 관측 로그를 남긴다.
2. `ThreadContext`에 누적 카운터(`aot_inline_cache_guard_reset_count`)를 추가하고
   retire 성공 시 누적한다.
3. 1이 검증된 뒤, `BuildAotCodeCacheImage`(`src/runtime/aot_code_cache.cpp`)의
   Task 243 `is_saved_register_return` dispatcher 가드를 제거해 epilogue RET의
   inline-cache fast-path를 복원한다.
4. Win32 x86 Debug 빌드(`build/win32_x86_dpmi`) 후 `pumpit1`을 `aot-dynamic`으로
   180초 구동해 다음을 확인한다.
   - `0x0304ED35` zero-EIP 종료가 재발하지 않는다.
   - guard 리셋이 실제 발생하고(관측 로그), return dispatcher/reentry가 Task 243
     기준(약 140만) 대비 감소한다.
   - progress가 기존 정체점(약 2.1만)을 유의미하게 상회한다.
5. 결과를 `docs/analysis/current-execution-frontier.md`, 작업 로그, `ARCHITECTURE.md`
   (해당 시)에 반영한다.

## 제약

- guest 코드와 Glide ABI는 변경하지 않는다.
- dispatcher의 스택 검색 복구는 도입하지 않는다(Task 243 금지 사항 유지).
- zero-EIP가 재발하면 3항을 되돌리고(가드 유지) 관측 결과를 frontier 문서에 기록한다.

# Task 245 Work Order: Implement Retire-Time Inline-Cache Guard Reset and Restore the Return Fast-Path

## Background

Task 243 blocked the saved-register epilogue zero-EIP termination by forcing the
breakpoint return dispatcher instead of the return inline cache. Every high-frequency
epilogue return now takes the exception path, so `aot-dynamic` progress stalls near 21
thousand while the trap backend reaches about 2.63 million in the same time.

The Task 238 design already confirmed the underlying defect: `RetireWin32AotGuestPage`
only rewrites the first byte of address-map entries on the retired page to `INT3`,
while learned inline-cache guards in other blocks keep jumping straight at the retired
entries. Each such hit traps on every transfer, and because the trap is not at a miss
tail, the source slot is never repatched. The design was approved but never
implemented (confirmed by code review on 2026-07-19).

## Work Items

1. Implement the Task 238 guard reset in `RetireWin32AotGuestPage`: within the same
   writable window, restore every installed guard (`0F 85`) whose guest-target
   immediate lies in the retired page to the initial miss form (`E9 rel32 → miss
   tail` + `90`), keeping the immediate and displacement. Report the count via
   `Win32AotGuestPageRetireResult::guard_reset_count` and sample to stderr.
2. Accumulate a `ThreadContext::aot_inline_cache_guard_reset_count` counter.
3. After 1 verifies, remove the Task 243 `is_saved_register_return` dispatcher guard
   in `BuildAotCodeCacheImage` to restore the epilogue RET inline-cache fast path.
4. Build Win32 x86 Debug and run `pumpit1` under `aot-dynamic` for 180 seconds:
   no `0x0304ED35` zero-EIP recurrence, observed guard resets, reduced return
   dispatcher/re-entry versus the ~1.4 million baseline, and progress well above the
   ~21 thousand stall.
5. Update `docs/analysis/current-execution-frontier.md`, the work log, and
   `ARCHITECTURE.md` as applicable.

## Constraints

- Do not change guest code or the Glide ABI.
- Do not introduce dispatcher stack scanning (Task 243 prohibition stands).
- If zero-EIP recurs, revert item 3 (keep the guard) and record the observations in
  the frontier document.
