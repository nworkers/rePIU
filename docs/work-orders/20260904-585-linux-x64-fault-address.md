# 작업 지시 20260904-585 — Linux x64 폴트 주소와 ESI/스택 불일치 원인 규명

설계: [20260904-585](../design/20260904-585-linux-x64-fault-address.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `include/repiu/engine/execution/thread_context.h` | `ShadowSelectorBlock` 구조체 및 포인터 추가 |
| `src/engine/execution/execution_trampoline.cpp` | 32비트 ShadowSelectorBlock 할당 및 수명 주기 관리 |
| `src/engine/aot/aot_runtime_dispatch.cpp` | `BuildAotSegmentTable`에서 32비트 ShadowSelector 주소 제공 |
| `src/engine/cpu_emul/instruction_emulation.cpp` | 세그먼트 셀렉터 갱신 시 `shadow_selectors` 동기화 |
| `docs/analysis/linux-port-frontier.md` | 3.32절 |
| `docs/work-logs/20260904-585-linux-x64-fault-address.md` | 작업 로그 |

## 구현 단계

1. `ShadowSelectorBlock`을 정의하고 `ThreadContext`에 추가합니다.
2. `ThreadContext` 초기화 시 4GiB 미만의 32비트 가상 메모리 공간에 `ShadowSelectorBlock`을 할당하고, 세그먼트 레지스터 갱신 지점에서 동기화합니다.
3. `BuildAotSegmentTable` 및 `aot_runtime_dispatch`에서 상위 32비트가 잘리지 않는 유효한 32비트 shadow selector 주소를 AOT 세그먼트 테이블에 바인딩합니다.
4. Linux x64 바이너리를 빌드하여 `0x010F1875`(`mov cl, es:[edi-1]`)가 SIGSEGV 없이 정상 실행되고, `0x010F1896`(`pop esi`)에서 ESI가 올바른 값(`0x00000000`)으로 복원되는지 확인합니다.
5. `0x010F18A4`(`mov eax, [esi]`)에서 환경 블록 접근이 정상적으로 이루어지는지 검증합니다.
6. 단위 테스트 및 프로브를 실행하여 회귀가 없는지 확인합니다.

## 검증 절차

1. `linux_x64_repiu` 빌드 후 `REPIU_GUEST_WATCH=0x010F1883 timeout 3s ./build/linux_x64_repiu/repiu pumpit2a` 실행하여 `esp == 0x0158CC80` 확인.
2. `REPIU_FAULT_EXIT_TRACE=1 timeout 3s ./build/linux_x64_repiu/repiu pumpit2a` 실행하여 `0x010F18A4`에서 ESI가 `0x00000000`으로 복원되는지 확인.
3. `repiu_core_probe` (Linux x64, Linux i386) 검증.
4. Win32 빌드 및 프로브 검증.

---

# Work order 20260904-585 — Investigate Linux x64 Fault Address and ESI/Stack Discrepancy

Design: [20260904-585](../design/20260904-585-linux-x64-fault-address.md)

## Files to change

| File | Change |
|---|---|
| `include/repiu/engine/execution/thread_context.h` | Add `ShadowSelectorBlock` struct and pointer |
| `src/engine/execution/execution_trampoline.cpp` | Allocate and manage lifecycle of 32-bit ShadowSelectorBlock |
| `src/engine/aot/aot_runtime_dispatch.cpp` | Bind valid 32-bit shadow selector address in `BuildAotSegmentTable` |
| `src/engine/cpu_emul/instruction_emulation.cpp` | Synchronize `shadow_selectors` on segment selector updates |
| `docs/analysis/linux-port-frontier.md` | Section 3.32 |
| `docs/work-logs/20260904-585-linux-x64-fault-address.md` | Work log |

## Implementation steps

1. Define `ShadowSelectorBlock` and integrate it into `ThreadContext`.
2. Allocate `ShadowSelectorBlock` in `< 4GiB` address space during `ThreadContext` initialization, and synchronize on segment register updates.
3. Bind the valid 32-bit shadow selector addresses in `BuildAotSegmentTable` and `aot_runtime_dispatch` without 64-bit pointer truncation.
4. Build Linux x64 binary and verify that `0x010F1875` (`mov cl, es:[edi-1]`) executes without SIGSEGV and `0x010F1896` (`pop esi`) restores ESI to `0x00000000`.
5. Verify that `0x010F18A4` (`mov eax, [esi]`) cleanly accesses the environment block.
6. Run unit tests and probes to ensure no regressions across platforms.

## Verification procedure

1. Run `REPIU_GUEST_WATCH=0x010F1883 timeout 3s ./build/linux_x64_repiu/repiu pumpit2a` and verify `esp == 0x0158CC80`.
2. Run `REPIU_FAULT_EXIT_TRACE=1 timeout 3s ./build/linux_x64_repiu/repiu pumpit2a` and inspect ESI at `0x010F18A4`.
3. Run `repiu_core_probe` on Linux x64 and i386.
4. Verify Win32 build and probes.

