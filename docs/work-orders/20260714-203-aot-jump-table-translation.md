# AOT Bounded Jump Table 번역 작업 지시서
# AOT Bounded Jump Table Translation Work Order

## 1. 작업 개요 (Task Overview)
* **목적:** Watcom switch 점프 테이블(`cmp reg,imm; ja; jmp dword ptr cs:[reg*4+disp32]`)을 AOT 번역 단위 안에서 네이티브로 실행하여, 레코드마다 dispatcher 탈출을 유발하는 현행 `kIndirectExit` 병목(사이클당 약 65,536회, 약 62초)을 제거합니다.
* **관련 문서:** `docs/design/20260714-aot-jump-table-translation.md`, `docs/analysis/current-execution-frontier.md` (2026-07-14 항목)

* **Goal:** Translate Watcom switch jump tables natively inside AOT translation units, removing the per-record dispatcher exits currently classified as `kIndirectExit` (~65,536 exits / ~62 s per decode cycle).
* **References:** `docs/design/20260714-aot-jump-table-translation.md`, `docs/analysis/current-execution-frontier.md` (2026-07-14 entry)

---

## 2. 세부 구현 대상 (Detailed Tasks)

### 1) aot_translation_plan.cpp — kJumpTable 분류
* `AotInstructionKind`에 `kJumpTable`을 추가하고 record에 테이블 target 목록을 보존합니다.
* 간접 분기 분류 지점에서 canonical 패턴 검사(FF /4 + reg*4 + disp32, 허용 프리픽스 `2E`/`3E`, 직전 `cmp reg,imm`+`ja`, register 일치)와 relocated 테이블 dword 범위 검증을 수행합니다.
* 검증 통과 시 각 테이블 target을 `pending`에 추가하고 `jump_table_count` 통계를 신설합니다. 실패 시 기존 `kIndirectExit`를 유지합니다.

### 2) aot_code_cache.cpp — 네이티브 분기 테이블 방출
* `kJumpTable` record에 대해 cache 내 native pointer table을 예약하고 `jmp dword ptr [index_reg*4 + native_table]`을 방출합니다 (CS override 제거).
* 각 엔트리를 대응 번역 블록으로 fixup하고, 미번역 target 엔트리는 INT3 dispatcher stub으로 채웁니다.

### 3) 통계·텔레메트리 노출
* `repiu_aot_probe` 출력과 로더 로그에 `jump_table_count` 및 방출된 native table 수를 노출합니다.

* Add `kJumpTable` classification with pattern/table validation and target enqueueing in `src/runtime/aot_translation_plan.cpp`; emit per-site native pointer tables with translated-block fixups and INT3 fallback entries in `src/runtime/aot_code_cache.cpp`; expose `jump_table_count` statistics through `repiu_aot_probe` and loader logs.

---

## 3. 검증 방법 (Verification Procedure)
* `scripts/build_win32_x86.ps1`로 win32_x86_debug 빌드가 오류 없이 통과함을 확인합니다.
* `repiu_aot_probe`에서 `indirect_exit_count` 감소와 `jump_table_count > 0`을 확인합니다.
* `REPIU_EXECUTION_TIMEOUT_MS=0`, `REPIU_EXECUTION_BACKEND=aot-dynamic` 환경에서 `repiu_supervisor_win32.exe pumpit1 120000`을 구동하여 다음을 확인합니다.
  1. `0x03086DAA` 집중 디스패치가 소멸하고 디코드 사이클이 대폭 단축된다.
  2. 새 예외/크래시 없이 실행이 지속되고, 가능하면 이후 frontier(그리기 게이트 도달 여부 포함)를 관찰한다.
* `dos4gw_hello` 등 기존 샘플 실행으로 회귀가 없음을 확인합니다.
