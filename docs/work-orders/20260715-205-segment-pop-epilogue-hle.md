# 세그먼트 POP 에필로그 HLE 작업 지시서
# Segment POP Epilogue HLE Work Order

## 1. 작업 개요 (Task Overview)
* **목적:** guest `0x030F5074` 세그먼트 복원 에필로그의 `POP ES`(및 같은 형태의 POP FS/GS)가 미처리 0xC0000005로 게스트 스레드를 종료시키는 현재 frontier를 제거합니다.
* **관련 문서:** `docs/design/20260715-segment-pop-epilogue-hle.md`, `docs/analysis/current-execution-frontier.md` (2026-07-15 항목)

* **Goal:** Remove the current frontier where the unhandled `POP ES` (and sibling POP FS/GS forms) in the segment-restore epilogue at guest `0x030F5074` kills the guest thread with 0xC0000005.
* **References:** `docs/design/20260715-segment-pop-epilogue-hle.md`, `docs/analysis/current-execution-frontier.md` (2026-07-15 entry)

---

## 2. 세부 구현 대상 (Detailed Tasks)

### 1) execution_trampoline.cpp — HandleSegmentPopInstruction 확장
* 기존 `1F`(POP DS) 단일 형태를 opcode 표 기반으로 확장합니다: `07`(ES, index 0, 길이 1), `1F`(DS, index 3, 길이 1), `0F A1`(FS, index 4, 길이 2), `0F A9`(GS, index 5, 길이 2).
* 게스트 스택 selector를 `RecordGuestSegmentLoad`로 shadow 기록하고 실제 host 세그먼트 레지스터는 적재하지 않습니다. `ESP += 4`, `EIP += 명령 길이`.
* `POP SS`(`17`)는 구현하지 않습니다(미관측, fail-closed 유지).

* Extend `HandleSegmentPopInstruction` from the single `1F` form to a small opcode table (`07`/`1F`/`0F A1`/`0F A9`), recording the popped selector through `RecordGuestSegmentLoad` shadow semantics and advancing `ESP`/`EIP`; keep `POP SS` unimplemented.

---

## 3. 검증 방법 (Verification Procedure)
* `scripts/build_win32_x86.ps1`로 win32_x86_debug 빌드가 오류 없이 통과함을 확인합니다.
* `REPIU_EXECUTION_BACKEND=aot-dynamic` 환경에서 `repiu_supervisor_win32.exe pumpit1 240000`을 구동하여 다음을 확인합니다.
  1. 약 150초 지점의 `original entry raised a caught exception` 게스트 스레드 종료가 사라진다.
  2. 150초 이후에도 디스패치/aot 카운터/샘플링 텔레메트리가 살아 있고 실행이 계속된다.
  3. 새 frontier가 나타나면 `docs/analysis/current-execution-frontier.md`에 기록한다.
* `dos4gw_hello` 실행으로 회귀가 없음을 확인합니다.
