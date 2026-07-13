# DPMI Real Mode Memory Block HLE 구현 작업 지시서
# DPMI Real Mode Memory Block HLE Implementation Work Order

## 1. 작업 개요 (Task Overview)
* **목적:** DPMI 인터럽트 `INT 31h` `AX=0x0100` 및 `AX=0x0101` HLE API 지원을 구현하여 게스트 코드 실행 흐름이 real mode memory 할당 단계에서 비정상적으로 종료되는 현상을 막습니다.
* **관련 문서:** `docs/design/20260714-dpmi-real-mode-memory-hle.md`

* **Goal:** Implement support for DPMI interrupt `INT 31h` `AX=0x0100` and `AX=0x0101` HLE APIs to prevent guest execution from crashing on real-mode memory allocation.
* **References:** `docs/design/20260714-dpmi-real-mode-memory-hle.md`

---

## 2. 세부 구현 대상 (Detailed Tasks)

### 1) ThreadContext 확장
* `src/platform/win32/execution_trampoline.cpp` 상단의 `struct ThreadContext` 내부에 `RealModeBlock` 구조체 정의와 `std::array<RealModeBlock, 32>` 배열, 그리고 `std::uint32_t dpmi_low_memory_bump_offset` 변수를 선언합니다.

* Declare the `RealModeBlock` struct, its array of size 32, and the `dpmi_low_memory_bump_offset` variable within `ThreadContext` inside `src/platform/win32/execution_trampoline.cpp`.

### 2) DPMI AX=0x0100 / AX=0x0101 핸들러 추가
* `execution_trampoline.cpp` 의 `HandleDpmiInterrupt31` 함수 내에 분기문을 확장하고, LDT descriptor 등록 및 selector 할당 API (`AllocateSelector`, `RegisterDescriptor`)와 메모리 offset 검증을 연동합니다.

* Extend the conditional branches in `HandleDpmiInterrupt31` inside `execution_trampoline.cpp` to integrate LDT descriptor registration and selector allocation APIs (`AllocateSelector`, `RegisterDescriptor`) with segment offset boundary checks.

---

## 3. 검증 방법 (Verification Procedure)
* `win32_x86_debug` 빌드를 다시 컴파일하고, `aot-dynamic` 모드 및 타임아웃 `0` 설정으로 가동합니다.
* spdlog 출력에서 DPMI `AX=0x100` 관련 에러가 발생하지 않고, 그 뒤의 DOS 파일 읽기나 기타 로직이 연이어 정상 진행되는지 검증합니다.

* Rebuild under the `win32_x86_debug` target and run with the `aot-dynamic` backend and timeout `0`.
* Confirm that the DPMI `AX=0x100` error disappears in the spdlog console output, and that subsequent guest execution logic continues.
