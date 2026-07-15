# 작업 지시: Resize HLE 크기 추적과 Allocator Heap 상한 모델링 (Task 213)

설계: `docs/design/20260716-213-resize-hle-allocator-ceiling.md`
브랜치: `feature/213-resize-hle-allocator-ceiling`

## 작업 항목 / Tasks

1. `[ ]` `ThreadContext` 구조체 확장 (`src/platform/win32/execution_trampoline.cpp`)
   - `last_dos_resize_requested_end` (std::uint32_t)
   - `last_dos_resize_allocator_end` (std::uint32_t)
2. `[ ]` `Win32ExecutionAttempt` 구조체 확장 (`include/repiu/platform/win32/execution_trampoline.h` & `src/platform/win32/execution_trampoline.cpp`)
   - `last_dos_resize_requested_end` (std::uint32_t)
   - `last_dos_resize_allocator_end` (std::uint32_t)
   - `Attempt` 복사 코드에 관련 필드 반영
3. `[ ]` `HandleDosResizeMemoryBlock` 시그니처 변경 및 로직 수정 (`src/platform/win32/execution_trampoline.cpp`)
   - `HandleDosResizeMemoryBlock(CONTEXT* win32_context, ThreadContext* context)` -> layout 및 selector_table 접근 가능
   - `guest_es`로 `SelectorTable`에서 base 조회
   - `limit_paragraphs = (context->linexe_arena_layout.dynamic_allocator_end - selector_base) / 16`
   - `BX > limit_paragraphs` 검사 및 carry flag / AX / BX 갱신
   - 하드코딩 `0xE700` 및 `0x4AE0` 제거
4. `[ ]` `ThreadContext`에 `dynamic_allocator_end` 연계 확인 (이미 `linexe_arena_layout`이 context에 내장되어 있으므로 직접 사용 가능)
5. `[ ]` `host/win32/main.cpp` 요약 로그에 신규 텔레메트리 2종(`last_dos_resize_requested_end`, `last_dos_resize_allocator_end`) 출력 추가
6. `[ ]` 빌드 및 OpenWatcom local sample suite 회귀 검증
7. `[ ]` `aot-dynamic` 백엔드 180초 구동을 통한 검증 (디코드 루프 통과 또는 정상 범위 내 heap 할당 제어 확인)
8. `[ ]` 작업 로그 작성

## 검증 기준 / Verification Criteria

* 빌드가 경고/에러 없이 통과한다.
* `dos4gw_hello` 등의 기본 샘플 실행이 정상 동작한다.
* PIU `aot-dynamic` 180초 실행 결과, 이전의 `0x045D7000` arena-end overflow 및 LINEXE private data 영역 훼손이 더 이상 발생하지 않는다.
* 텔레메트리 요약 출력에 resize 요청 주소 상한 정보가 올바르게 로깅된다.

---

# Work Order: Resize HLE Paragraph Tracking & Allocator Heap Ceiling Modeling (Task 213)

Design: `docs/design/20260716-213-resize-hle-allocator-ceiling.md`
Branch: `feature/213-resize-hle-allocator-ceiling`

## Tasks

1. `[ ]` Expand `ThreadContext` with `last_dos_resize_requested_end` and `last_dos_resize_allocator_end`.
2. `[ ]` Expand `Win32ExecutionAttempt` with matching fields and update copying logic in `execution_trampoline.cpp`.
3. `[ ]` Modify `HandleDosResizeMemoryBlock` in `execution_trampoline.cpp`:
   - Retrieve selector base from `context->selector_table`.
   - Calculate limit paragraphs relative to `context->linexe_arena_layout.dynamic_allocator_end`.
   - Implement strict conditional boundaries checking against `BX` with carry/AX/BX updates.
   - Remove hardcoded `0xE700` and `0x4AE0` checks.
4. `[ ]` Ensure `linexe_arena_layout` is correctly used (already present in `ThreadContext`).
5. `[ ]` Add the two new telemetry fields to the exit summary prints in `src/host/win32/main.cpp`.
6. `[ ]` Build and verify with OpenWatcom local samples.
7. `[ ]` Run a 180-second `aot-dynamic` supervisor run to ensure decode loop crash is resolved.
8. `[ ]` Write work log.

## Verification Criteria
- Rebuild completes with zero errors.
- Basic Watcom CLIB samples continue to run successfully.
- `aot-dynamic` run survives past 147s or demonstrates dynamic bounds enforcement without corrupting the LINEXE area or crashing via `0x045D7000` overflow.
