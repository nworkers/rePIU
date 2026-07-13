# 20260714-202-glide-drawing-stubs-hle-log

## 작업 개요 (Task Summary)
* **작업 대상:** Glide 핵심 그리기 API 5종 HLE stub — `_GRDRAWPOINT@4`(71), `_GRDRAWTRIANGLE@12`(73), `_GRDRAWPLANARPOLYGON@12`(74), `_GRDRAWPLANARPOLYGONVERTEXLIST@8`(75), `_GRDRAWPOLYGON@12`(76)
* **목적:** 미등록 그리기 게이트 호출로 인한 예외 크래시를 전방위 예방하여 렌더링 루틴 진입 시 실행이 계속되도록 함
* **관련 문서:** `docs/design/20260714-glide-drawing-stubs-hle.md`, `docs/work-orders/20260714-202-glide-drawing-stubs-hle.md`

---

## 작업 내용 (Detailed Changes)

### 1) glide_hle.cpp 시그니처 등록
* `kObservedSignatures` 배열을 26 → 31개로 확장하고 그리기 API 5종의 인자 바이트 수와 `kVoid` 반환 종별 메타데이터를 추가하였습니다.

### 2) execution_trampoline.cpp 디스패처 stub
* `HandleGlideGateBoundary`에 5종 분기를 추가하여 반환 주소 복귀와 스택 정리(인자 4바이트 → `Esp += 8`, 8바이트 → `Esp += 12`, 12바이트 → `Esp += 16`)를 수행하는 pass-through stub을 구성하였습니다. 실제 렌더링은 아직 수행하지 않습니다.

---

## 검증 결과 (Verification Results)
* **빌드 검증:** win32_x86_debug 전체 빌드가 오류 없이 통과하였습니다.
* **런타임 검증:** 40초 supervisor 구동에서 새 예외 크래시는 없었습니다. 다만 이번 실행의 마지막 Glide 호출은 ordinal `0x5E` `_GRCULLMODE@4`(94)에 머물렀고, 그리기 게이트(ordinal 73 등)까지는 아직 도달하지 않았습니다. OVL resident-name 테이블 재확인으로 ordinal ↔ 이름 매핑(71~77 그리기 API, 85 `_GRBUFFERSWAP@4`, 89 `_GRCLIPWINDOW@16`, 94 `_GRCULLMODE@4`)을 확정하였습니다. 참고로 ordinal 77 `_GRDRAWPOLYGONVERTEXLIST@8`은 아직 stub 미등록 상태입니다.
* **평가:** stub 등록·스택 정리 코드는 자리에 있으나 "그리기 게이트가 실제로 터지는지"는 이번 실행 범위에서 관측되지 않았습니다. 게스트는 grCullMode 이후 EIP `0x03086DAA`의 비-Glide 디스패치 루프(초당 약 1,140회)에서 대기 중이며, 이 루프가 무엇을 기다리는지 규명하는 것이 다음 프론티어입니다.

---

## Task Summary
* **Task:** HLE stubs for the five core Glide drawing APIs (`_GRDRAWPOINT@4`, `_GRDRAWTRIANGLE@12`, `_GRDRAWPLANARPOLYGON@12`, `_GRDRAWPLANARPOLYGONVERTEXLIST@8`, `_GRDRAWPOLYGON@12`)
* **Changes:** Extended `kObservedSignatures` to 31 entries in `src/hle/glide_hle.cpp` and added pass-through dispatch branches with correct stack cleanup (8/12/16 bytes) in `HandleGlideGateBoundary` in `src/platform/win32/execution_trampoline.cpp`.

## Verification
* Full win32_x86_debug build passed. A 40-second supervised run produced no new crash; however, the last observed Glide call remained ordinal `0x5E` `_GRCULLMODE@4` — the drawing gates were not yet reached in this run. The OVL resident-name table re-parse confirmed the ordinal-to-name mapping (71–77 drawing APIs, 85 `_GRBUFFERSWAP@4`, 89 `_GRCLIPWINDOW@16`, 94 `_GRCULLMODE@4`); note that ordinal 77 `_GRDRAWPOLYGONVERTEXLIST@8` has no stub yet. The stubs are in place, but exercising them depends on resolving what the post-grCullMode dispatch loop at EIP `0x03086DAA` is waiting for, which is the next frontier.
