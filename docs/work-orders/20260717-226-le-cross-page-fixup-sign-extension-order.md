# 작업 지시: LE cross-page fixup 부호 확장 수정
# Work Order: LE Cross-Page Fixup Sign-Extension Fix

## 목표 (Goal)

Tasks 221-225 frontier의 근인인 LE fixup `source_offset` 부호 미확장 버그를 수정해,
게스트 명령 `mov edx,[esp+0x154]`(guest `0x03021FFD`) 손상과 그로 인한
`0xDD1523B1` 파일명 복사 crash를 제거한다.

## 범위 (Scope)

1. `src/exe/executable_headers.cpp` `ApplyLeInternalRelocations`: source offset
   계산을 `int16_t` 부호 확장으로 변경, 음수 결과 방어.
2. `src/runtime/runtime_memory.cpp` `FindSourceObjectForPage`(런타임 이미지
   fixup 재적용 공유 헬퍼): 동일 수정.
3. 회귀 방지: 일반 fixup(`source_offset < 0x8000`)에는 무영향, cross-page
   fixup(음수)만 올바른 위치로 이동.

## 비범위 (Out of Scope)

* 수정 후 드러나는 새 frontier(`0x030F7A0C`, fault VA `0x4091`)는 별도 task.
* fixup source type별 width 처리(16:16 vs 32-bit)는 이번 변경 대상 아님.

## 검증 (Verification)

* `repiu_aot_probe <PIU.EXE> 0x01021FFD`가 `mov edx, [esp+0x154]`로 복원.
* `repiu_supervisor_win32 pumpit1`(aot-dynamic)에서 `0xDD1523B1` crash 소멸,
  `dispatch_entry`가 이전(63446)보다 크게 전진.
* 빌드: `build/win32_x86_dpmi`(VS 2026 Community 번들 cmake, Win32).

## 산출물 (Deliverables)

* 코드 수정 2곳.
* 설계 `docs/design/20260717-226-le-cross-page-fixup-sign-extension.md`.
* 작업 로그 `docs/work-logs/20260717-226-le-cross-page-fixup-sign-extension-log.md`.
* kb 갱신 `docs/kb/le-format-and-relocation.md`.
* analysis 갱신 `docs/analysis/current-execution-frontier.md`.

---

**English summary.** Fix the root cause of the Tasks 221-225 frontier: the LE fixup
`source_offset` was applied without sign extension, so a cross-page fixup
(`source_offset=0xFFFF` = signed -1) corrupted the guest instruction
`mov edx,[esp+0x154]` at guest `0x03021FFD`, producing the `0xDD1523B1` filename-copy
crash. Sign-extend `source_offset` (`int16_t`) in both `ApplyLeInternalRelocations`
(`executable_headers.cpp`) and the runtime helper `FindSourceObjectForPage`
(`runtime_memory.cpp`), guarding against negative results. Verify via `repiu_aot_probe`
(instruction restored) and a `pumpit1` run (crash gone, execution advances). The new
frontier at `0x030F7A0C` is out of scope.
