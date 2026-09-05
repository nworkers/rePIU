# 작업 지시 20260905-588 — Linux x64 full RIP attribution

설계: [20260905-588](../design/20260905-588-linux-x64-full-rip-attribution.md)

## 변경 파일

| 파일 | 변경 |
|---|---|
| `src/platform/linux/fault_handler.cpp` | 미처리 fault 로그에 full host RIP 추가 |
| `ARCHITECTURE.md` | Linux x64 fault 진단 필드의 역할 갱신 |
| `docs/analysis/linux-port-frontier.md` | 관측 결과와 다음 분기 기록 |
| `docs/analysis/linux-x64-fault-context.md` | EIP/RIP 안전 범위와 관측 계약 갱신 |
| `docs/work-logs/20260905-588-linux-x64-full-rip-attribution.md` | 결과와 검증 기록 |

## 구현 순서

1. async-signal-safe formatter가 full RIP를 받도록 확장한다.
2. signal context의 RIP를 미처리 fault 경로에서 전달한다.
3. Linux x64 executable을 빌드하고 `pumpit2a`를 재현한다.
4. full RIP/eip 관계를 분석 문서와 작업 로그에 기록하고 커밋한다.

## 검증

1. `cmake --build build/linux_x64_debug --target repiu --parallel 4` (WSL)
2. `REPIU_GUEST_WATCH=0x010F4A96 timeout 5s build/linux_x64_debug/repiu pumpit2a` (WSL)

---

# Work order 20260905-588 — Linux x64 full-RIP attribution

Design: [20260905-588](../design/20260905-588-linux-x64-full-rip-attribution.md)

## Files to change

| File | Change |
|---|---|
| `src/platform/linux/fault_handler.cpp` | Add full host RIP to unhandled-fault logging |
| `ARCHITECTURE.md` | Update the Linux x64 fault diagnostic contract |
| `docs/analysis/linux-port-frontier.md` | Record results and the next branch |
| `docs/analysis/linux-x64-fault-context.md` | Update EIP/RIP safety and observation contract |
| `docs/work-logs/20260905-588-linux-x64-full-rip-attribution.md` | Record result and verification |

## Implementation order

1. Extend the async-signal-safe formatter to accept full RIP.
2. Pass RIP from the signal context along the unhandled-fault path.
3. Build the Linux x64 executable and reproduce `pumpit2a`.
4. Record the full-RIP/EIP relation in analysis and the work log, then commit.

## Verification

1. `cmake --build build/linux_x64_debug --target repiu --parallel 4` (WSL)
2. `REPIU_GUEST_WATCH=0x010F4A96 timeout 5s build/linux_x64_debug/repiu pumpit2a` (WSL)
