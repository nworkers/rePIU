# 20260801-385 Port-I/O 전용 DBT Dispatch 작업 로그 / Work Log

설계: [20260801-385-port-io-specific-dbt-dispatch.md](../design/20260801-385-port-io-specific-dbt-dispatch.md)

작업 지시: [20260801-385-port-io-specific-dbt-dispatch.md](../work-orders/20260801-385-port-io-specific-dbt-dispatch.md)

## 한국어

### 구현

- `enable_dbt_port_io_dispatch`를 code-cache option/image와 Win32 placement에 추가하고 dynamic append까지 전달했습니다.
- `kPortIo`를 일반 `kHleBoundary` switch와 분리했습니다. 전체 HLE dispatch 또는 Port-I/O 전용 dispatch가 활성일 때만 기존 fail-closed host-stack slot을 방출합니다.
- coverage validator가 Port-I/O dispatch slot의 layout과 fallback fixup을 검사하도록 확장했습니다.
- synthetic probe는 Port-I/O가 dispatch slot을 받는 동안 일반 HLE boundary가 INT3로 남는 격리 조건을 검증합니다.
- `REPIU_AOT_DBT_PORT_IO_DISPATCH=1|on|true` opt-in과 상태 로그를 추가했습니다. 기본값은 off입니다.

### 검증

- Release Win32 `repiu_aot_probe`와 loader 빌드가 성공했습니다. 기존 C4819/LNK4217 경고만 남았습니다.
- 전체 probe는 종료 코드 0이며 `port_io_dispatch_specific=true`, 기존 superblock 검증 전체 true, `selector_guard_all=true`입니다.
- 동일 EEPROM 복사본의 5초 비교에서 양쪽 모두 166 buffer swap을 기록했습니다.
- 기준/활성 Port I/O는 6,837/6,861회로 거의 같고 모두 handled였습니다.
- 활성 실행은 정상 timeout했으며 host dispatch entry/attempt/success/fallback은 `6,784/6,784/6,778/6`입니다. 여섯 unhandled 건은 기존 INT3 fallback으로 복구되었습니다.
- 전체 예외는 68,386회에서 61,300회(-10.36%), breakpoint는 40,333회에서 33,711회(-16.42%), hotspot HLE outcome은 24,815회에서 18,057회(-27.23%)로 감소했습니다.

짧은 동일-frame 스모크에서 기능 안전성과 예외 제거가 확인됐습니다. 과거 전체 HLE dispatch의 렌더링 회귀와 분리하기 위해 기본값은 아직 off이며, Music Select 수동 capture 후 승격 여부를 결정합니다.

## English

### Implementation

- Added `enable_dbt_port_io_dispatch` to code-cache options/images and Win32 placement, including dynamic append propagation.
- Split `kPortIo` from the ordinary `kHleBoundary` switch. It emits the existing fail-closed host-stack slot only when general HLE dispatch or Port-I/O-specific dispatch is enabled.
- Extended coverage validation for Port-I/O dispatch layout and fallback fixup.
- Added a synthetic isolation probe proving Port-I/O receives a dispatch slot while ordinary HLE remains INT3.
- Added the `REPIU_AOT_DBT_PORT_IO_DISPATCH=1|on|true` opt-in and status log. Default remains off.

### Verification

- Release Win32 `repiu_aot_probe` and loader builds passed with only pre-existing C4819/LNK4217 warnings.
- The full probe exited zero with `port_io_dispatch_specific=true`, all existing superblock checks true, and `selector_guard_all=true`.
- Both sides of a five-second identical-EEPROM comparison produced 166 buffer swaps.
- Baseline/enabled Port I/O counts were 6,837/6,861, nearly identical and fully handled.
- The enabled run timed out normally. Host dispatch entry/attempt/success/fallback was `6,784/6,784/6,778/6`; the six unhandled cases recovered through the existing INT3 fallback.
- Total exceptions fell from 68,386 to 61,300 (-10.36%), breakpoints from 40,333 to 33,711 (-16.42%), and hotspot HLE outcomes from 24,815 to 18,057 (-27.23%).

The short same-frame smoke confirms safety and exception removal. Default remains off pending a manual Music Select capture, keeping this path isolated from the historical rendering regression of general HLE dispatch.
