# 20260714-200-dpmi-real-mode-memory-hle-log

## 작업 개요 (Task Summary)
* **작업 대상:** DPMI INT 31h AX=0x0100 (Allocate Real Mode Memory) / AX=0x0101 (Free Real Mode Memory) HLE 및 Glide _GRDRAWLINE@8 (ordinal 72) HLE 지원
* **목적:** 게스트 x86 초기화 도중 DPMI Real Mode Memory 할당 누락 및 미등록 Glide API 호출로 인한 크래시 중단 현상을 해결
* **결과:** 크래시 없이 게스트 코드 실행 흐름이 26초 이상 완벽하게 통과하여 메인 Idle 대기 루프에 안착하는 데 성공

---

## 작업 내용 (Detailed Changes)

### 1) DPMI Real Mode Memory Block HLE 이식
* `ThreadContext` 내부에 16비트 리얼 모드 메모리 풀 할당 이력을 추적할 `RealModeBlock` 구조와 최대 32개 추적 배열을 정의하였습니다.
* `dpmi_low_memory_bump_offset` 멤버를 추가하여 conventional memory 영역인 `0x1000U` 오프셋부터 paragraph (16바이트) 단위로 정렬 할당되는 Bump Allocator를 구현하였습니다.
* `HandleDpmiInterrupt31` 함수 내에 `AX=0x0100` 및 `AX=0x0101` 핸들러 분기를 신설하여, DPMI 명세서 규격에 맞게 LDT descriptor 등록 및 selector 매핑을 처리하였습니다.

### 2) Glide _GRDRAWLINE@8 API HLE 이식
* 로더 실행 도중 ordinal 72번의 미등록 Glide trap gate 호출이 관측되어, OVL export 테이블 리버스 엔지니어링 분석을 통해 `_GRDRAWLINE@8` (인자 8바이트, 리턴 없음) 임을 찾아내었습니다.
* `glide_hle.cpp` 에 시그니처 정보를 추가하고 `execution_trampoline.cpp` 디스패처 분기에 8바이트 스택 정리(`Esp += 12`) stub을 구성하여 게스트 스택 불일치 없이 무사 통과시켰습니다.

---

## 회고 및 결과 (Retrospective & Verification Results)
* **빌드 검증:** CMake 디버그 빌드를 다시 실행하여 컴파일 경고나 링크 오류가 없음을 확인하였습니다.
* **런타임 동작:** `REPIU_EXECUTION_TIMEOUT_MS="0"` 및 `REPIU_EXECUTION_BACKEND="aot-dynamic"` 환경에서 로더를 실행하여, 기존의 DPMI 및 미등록 Glide 에러 크래시가 완벽히 소멸하고 에뮬레이터가 26초 이상 무한 대기 루프(Idle) 상태에 성공적으로 돌입하는 것을 텔레메트리 덤프를 통해 관측 및 검증하였습니다.

---

## Task Summary
* **Task:** DPMI INT 31h AX=0x0100 (Allocate Real Mode Memory) / AX=0x0101 (Free Real Mode Memory) HLE and Glide _GRDRAWLINE@8 (ordinal 72) HLE Support
* **Goal:** Resolve crashes during guest x86 initialization due to missing DPMI Real Mode Memory allocation and unregistered Glide API calls.
* **Result:** Successfully bypassed all initialization blockers; execution flow proceeds completely without crashes for over 26 seconds, settling into the main Idle loop.

---

## Detailed Changes
* **DPMI Real Mode Memory Block HLE:** Added `RealModeBlock` tracking structures and `dpmi_low_memory_bump_offset` (starting at `0x1000U`) in `ThreadContext`. Implemented `AX=0x0100` (Allocation) and `AX=0x0101` (Free) handlers in `HandleDpmiInterrupt31` to allocate LDT descriptors and return proper segment/selector values.
* **Glide _GRDRAWLINE@8 HLE:** Identified ordinal 72 as `_GRDRAWLINE@8` through OVL resident-name table parsing. Added its signature to `glide_hle.cpp` and integrated an 8-byte stack cleanup stub in the HLE dispatcher within `execution_trampoline.cpp` to prevent stack misalignment.
