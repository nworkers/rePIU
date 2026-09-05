# 작업 지시 20260905-587 — Linux x64 세그먼트 PUSH HLE

설계: [20260905-587](../design/20260905-587-linux-x64-segment-push-hle.md)

## 변경 파일

| 파일 | 변경 |
|---|---|
| `src/engine/cpu_emul/instruction_emulation.h` | segment PUSH HLE 선언 추가 |
| `src/engine/cpu_emul/instruction_emulation.cpp` | selector를 guest stack에 저장하는 HLE 구현 |
| `src/engine/execution/execution_trampoline.cpp` | 기존 segment HLE chain에 PUSH handler 연결 |
| `docs/analysis/linux-port-frontier.md` | Task 585 해결 및 새 frontier 기록 |
| `docs/work-logs/20260905-587-linux-x64-segment-push-hle.md` | 작업 결과와 검증 기록 |

## 구현 순서

1. 지원 opcode와 selector shadow를 대응시킨다.
2. guest writable stack range를 검증한 뒤 zero-extended selector dword를 저장한다.
3. ESP/EIP만 갱신하고 HLE chain에 연결한다.
4. Linux x64 core probe 및 `pumpit2a` 실행으로 SIGILL 제거와 다음 frontier를 확인한다.
5. 분석 문서와 작업 로그를 갱신하고 작업 단위를 커밋한다.

## 검증

1. `cmake --build /tmp/repiu-linux-x64-debug --target repiu_core_probe --parallel 2`
2. `/tmp/repiu-linux-x64-debug/repiu_core_probe`
3. `timeout 5s /tmp/repiu-linux-x64-debug/repiu pumpit2a` (Linux x64)

---

# Work order 20260905-587 — Linux x64 Segment PUSH HLE

Design: [20260905-587](../design/20260905-587-linux-x64-segment-push-hle.md)

## Files to change

| File | Change |
|---|---|
| `src/engine/cpu_emul/instruction_emulation.h` | Declare segment PUSH HLE |
| `src/engine/cpu_emul/instruction_emulation.cpp` | Emulate storing a selector on the guest stack |
| `src/engine/execution/execution_trampoline.cpp` | Wire the PUSH handler into the existing segment HLE chain |
| `docs/analysis/linux-port-frontier.md` | Record the Task 585 resolution and new frontier |
| `docs/work-logs/20260905-587-linux-x64-segment-push-hle.md` | Record results and verification |

## Implementation order

1. Map supported opcodes to selector shadows.
2. Validate the writable guest-stack range and store the zero-extended selector dword.
3. Update only ESP/EIP and connect the handler to the HLE chain.
4. Verify the removed SIGILL and next frontier with the Linux x64 core probe and `pumpit2a`.
5. Update analysis and work log, then commit the task unit.

## Verification

1. `cmake --build /tmp/repiu-linux-x64-debug --target repiu_core_probe --parallel 2`
2. `/tmp/repiu-linux-x64-debug/repiu_core_probe`
3. `timeout 5s /tmp/repiu-linux-x64-debug/repiu pumpit2a` (Linux x64)
