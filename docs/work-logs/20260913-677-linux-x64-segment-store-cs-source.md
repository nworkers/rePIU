# Task 677 작업 로그: Linux x64 segment store CS source

## 한국어

`MOV AX,CS` 같은 `8C /r` source CS form을 host CS로 처리하지 않고, 현재
guest EIP를 포함하는 selector-table code descriptor에서 logical guest CS를
조회하도록 했습니다. 조회가 실패하면 selector 0을 기록하지 않고 HLE를
거부하도록 fail-closed했습니다.

검증 결과:

- `repiu_core_probe`: `core_probe_total=27`, `core_probe_failures=0`
- focused segment probe: `cs_source_store=true`, `cs_source_missing_refused=true`
- Linux x64 `repiu` 빌드 성공
- 15초 clean smoke에서 SIGTRAP/SIGSEGV 재발 없음
- heartbeat는 24에서 멈췄으며 마지막 host EIP는 `0x20053955`
- 동적 cache decode에서 `0x200539EC -> 0x20053955` 명시적 guest loop 확인

마지막 loop는 깨진 return thunk가 아니라 번역된 back-edge로 확인되었습니다.
정상 화면/입력 진행과 정상 종료는 아직 미확정이며, 이 작업에서는 이를
주소별 예외로 우회하지 않았습니다.

## English

`8C /r` source-CS forms such as `MOV AX,CS` now resolve logical guest CS from
the selector-table code descriptor containing the current guest EIP instead of
using host CS. If lookup fails, the handler refuses the operation rather than
recording selector zero.

Verification:

- `repiu_core_probe`: `core_probe_total=27`, `core_probe_failures=0`
- Focused segment probe: `cs_source_store=true`, `cs_source_missing_refused=true`
- Linux x64 `repiu` build succeeded
- A 15-second clean smoke run reproduced neither SIGTRAP nor SIGSEGV
- Heartbeat stopped at 24 with last host EIP `0x20053955`
- Dynamic cache decoding showed an explicit guest loop `0x200539EC -> 0x20053955`

The final loop is a translated back-edge, not a corrupted return thunk. Normal
screen/input progress and clean termination remain unresolved; no address-
specific exception was added to bypass it.
