# Task 723 작업 로그 — Linux x64 Glide host-work ordinal 분해

설계: [20260919-723](../design/20260919-723-linux-x64-glide-host-work-ordinal.md) ·
작업 지시: [20260919-723](../work-orders/20260919-723-linux-x64-glide-host-work-ordinal.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260919-722](20260919-722-linux-x64-full-rip-stall-trace.md)

## 결과

기존 execution/ordinal profile의 Linux x64 30초 `pumpit2a` summary는 host work
25,768,579,029 cycles를 기록했습니다. `grBufferSwap` 16,206,075,482 cycles(62.9%)와
`grLfbLock` 6,272,964,560 cycles(24.3%)가 합계 87.2%를 차지했습니다. 일반 rendezvous
wake는 2,258,755,174 cycles이므로 지배항이 아닙니다.

`REPIU_GLIDE_SWAP_TIME_PROFILE=1`을 추가한 실험은 완결 summary 전에 제한 시간을 초과해
중단했습니다. 수치를 사용하지 않았으며, swap profile 조합의 종료 지연은 다음 조사 항목입니다.

## 검증

- Linux x64 `pumpit2a`, 30초 timeout, execution/ordinal profile: timeout immediate-exit와
  ordinal summary 완료
- 제품 소스와 Win32 x86 구성 변경 없음

---

## English

Design: [20260919-723](../design/20260919-723-linux-x64-glide-host-work-ordinal.md) ·
Work order: [20260919-723](../work-orders/20260919-723-linux-x64-glide-host-work-ordinal.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260919-722](20260919-722-linux-x64-full-rip-stall-trace.md)

### Result

The existing execution/ordinal profile's 30-second Linux x64 `pumpit2a` summary recorded
25,768,579,029 host-work cycles. `grBufferSwap` consumed 16,206,075,482 cycles (62.9%)
and `grLfbLock` 6,272,964,560 (24.3%), totaling 87.2%. General rendezvous wake consumed
2,258,755,174 cycles and is not dominant.

An experiment additionally enabling `REPIU_GLIDE_SWAP_TIME_PROFILE=1` exceeded its bounded
observation time before producing a complete summary. Its numbers were excluded; exit delay
with that profile combination is a next investigation item.

### Verification

- Linux x64 `pumpit2a`, 30-second timeout, execution/ordinal profile: completed timeout
  immediate-exit and ordinal summary.
- No product-source or Win32 x86 configuration change.
