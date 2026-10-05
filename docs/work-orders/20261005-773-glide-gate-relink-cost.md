# Task 773 작업 지시: Glide gate 재연결 비용 없애기

설계: [20261005-773](../design/20261005-773-glide-gate-relink-cost.md)

## 절차

1. `include/repiu/engine/aot_glide_gate_fixup_index.h`, `src/engine/aot_glide_gate_fixup_index.cpp`:
   `AotGlideGateFixupIndex`, `EnsureAotGlideGateFixupIndex`, `FindAotGlideGateFixupOffsets`,
   `CollectGlideGateFixupWrites`. `CMakeLists.txt`의 `repiu_exe`에 추가.
2. `AotCodeCachePlacement`에 `glide_gate_fixup_index` 멤버.
3. `ActivateGlideGateDirectTarget`: inline cache site 탐색 제거, 색인 조회와 "다른 값만 쓰기"로 교체, 쓸 것이 없으면
   보호 변경 없이 `true`. 카운터는 실제 쓴 수.
4. `src/tools/aot_probe/glide_gate_fixup_index_probe.{h,cpp}`: core probe(모든 호스트)와 Win32 AOT probe 목록에 등록.
5. 문서: `ARCHITECTURE.md`의 Glide gate direct dispatch 절(재연결 설명 정정, 색인), `linux-port-frontier.md`, 작업 로그.
6. 빌드: Linux x64 Debug, Linux i386 Release(`build/linux_i386_release_g`). core probe.
7. 실기: Linux i386 60초(`WAYLAND_DISPLAY=repiu-none`)를 Task 772와 같은 조건으로 여러 번, 스레드 CPU와 `[repiu-live-gdd]`.
   Linux x64 pumpit1 회귀.

## 범위 변경 (구현 중)

probe가 Linux i386에서 gate로 가는 slot이 rel32 범위 검사에 걸려 한 번도 쓰이지 않았음을 드러냈습니다. 설계에 결정 5(direct
모델에서는 변위를 2³²로 감아 씀)를 더하고 같은 작업에서 구현했습니다. 사용자가 승인한 계획의 2번(원인 없애기)에 해당합니다.

## 완료 조건

probe가 통과하고, i386 정상 상태의 게스트 스레드 CPU와 Activate 회당 비용이 줄었음이 측정으로 남으며, 느린 상태 진입이
Task 772보다 줄었는지(또는 줄지 않았는지)가 기록됩니다.

---

# Task 773 Work Order: Removing the Cost of the Glide Gate Relink

Design: [20261005-773](../design/20261005-773-glide-gate-relink-cost.md)

## Steps

1. `include/repiu/engine/aot_glide_gate_fixup_index.h`, `src/engine/aot_glide_gate_fixup_index.cpp`:
   `AotGlideGateFixupIndex`, `EnsureAotGlideGateFixupIndex`, `FindAotGlideGateFixupOffsets`,
   `CollectGlideGateFixupWrites`. Add them to `repiu_exe` in `CMakeLists.txt`.
2. A `glide_gate_fixup_index` member in `AotCodeCachePlacement`.
3. `ActivateGlideGateDirectTarget`: remove the inline cache site scan, replace it with the index lookup and "write only what
   differs", and return `true` without a protection change when there is nothing to write. Counters count actual writes.
4. `src/tools/aot_probe/glide_gate_fixup_index_probe.{h,cpp}`: registered in the core probe (every host) and the Win32 AOT
   probe.
5. Documents: the Glide gate direct dispatch section of `ARCHITECTURE.md` (correct the relink description, the index),
   `linux-port-frontier.md`, the work log.
6. Builds: Linux x64 Debug, Linux i386 Release (`build/linux_i386_release_g`). Core probe.
7. Real hardware: Linux i386 for 60 s (`WAYLAND_DISPLAY=repiu-none`) several times under Task 772's conditions, thread CPU
   and `[repiu-live-gdd]`. Linux x64 pumpit1 regression.

## Scope change (during implementation)

The probe showed that on Linux i386 slots going to gates failed the rel32 range check and were never written. Decision 5
(on the direct model the displacement wraps at 2³²) was added to the design and implemented in the same task. It is item 2
(removing the cause) of the plan the user approved.

## Done when

The probe passes; measurements show the guest thread's CPU and the per-call cost of Activate falling in i386's normal state;
and whether entries into the slow state fell compared with Task 772 (or did not) is recorded.
