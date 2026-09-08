# Task 643 작업 로그: AOT guest map 문맥 추적

설계: [20260908-643](../design/20260908-643-aot-guest-map-context.md)  
작업 지시: [20260908-643](../work-orders/20260908-643-aot-guest-map-context.md)  
분석: [linux-port-frontier 3.80](../analysis/linux-port-frontier.md)

## 한국어

`REPIU_AOT_GUEST_MAP_CONTEXT`를 추가했습니다. 기존 guest-map 주소가 일치하면 지정
반경의 인접 address-map entry를 원본 guest bytes와 emitted bytes까지 출력합니다.
반경은 최대 64이며, 설정하지 않으면 기존 동작과 출력이 유지됩니다.

실제 문맥에서 `0x010F1D74..0x010F1D7F` prologue와
`0x010F1E48..0x010F1E56` epilogue가 40바이트씩 정확히 대칭임을 확인했습니다. 그러나
single-step watch에서 일반 PUSH 다섯 개 동안 guest ESP는 `0x0158CC70`으로
고정됐고, `PUSH ES` HLE부터 정상적으로 `0x0158CC6C`으로 감소했습니다. 최초 불일치는
`0x010F1D74 PUSH EBX`, 누락은 총 20바이트입니다.

### 검증

* Linux x64 `repiu`, `repiu_core_probe` 빌드: 통과
* Linux x64 core probe: `24/24`, failures `0`
* 실제 map context: 원본·emitted prologue/epilogue 확인
* 세 실제 watch: `PUSH EBX`, `PUSH EDI`에서 ESP 불변, `PUSH ES` HLE에서 `-4`

게임은 아직 정상 실행되지 않습니다. 다음 작업은 x64 legacy fallback의 일반
PUSH/POP을 guest-stack 의미로 처리하는 공용 설계와 구현입니다.

## English

Added `REPIU_AOT_GUEST_MAP_CONTEXT`. Once an existing guest-map address
matches, it prints neighboring address-map entries within a bounded radius,
including original guest and emitted bytes. The radius is capped at 64; absent
the setting, existing behavior and output remain unchanged.

The real context confirms a balanced 40-byte prologue at
`0x010F1D74..0x010F1D7F` and epilogue at `0x010F1E48..0x010F1E56`. Actual
single-step watches show guest ESP fixed at `0x0158CC70` across five general
pushes, then correctly reduced to `0x0158CC6C` by `PUSH ES` HLE. The first
mismatch is `PUSH EBX` at `0x010F1D74`; the total missing decrement is 20 bytes.

### Verification

* Linux x64 `repiu` and `repiu_core_probe` build: passed
* Linux x64 core probe: `24/24`, failures `0`
* Real map context: original/emitted prologue and epilogue confirmed
* Three real watches: unchanged ESP at `PUSH EBX` and `PUSH EDI`, `-4` at `PUSH ES` HLE

The game still does not run normally. The next task is a shared design and
implementation for guest-stack semantics of general PUSH/POP during x64 legacy
fallback.
