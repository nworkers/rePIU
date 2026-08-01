# 20260801-386 Port-I/O Dispatch 기본 활성화 작업 로그 / Work Log

설계: [20260801-386-port-io-dispatch-default-promotion.md](../design/20260801-386-port-io-dispatch-default-promotion.md)

작업 지시: [20260801-386-port-io-dispatch-default-promotion.md](../work-orders/20260801-386-port-io-dispatch-default-promotion.md)

## 한국어

### 캡처 판단

- 비교 기준은 guarded segment-read 캡처 32.835초/1,018 frame과 Port-I/O dispatch 캡처 37.204초/1,307 frame입니다.
- 고정 초기화 표식은 양쪽 모두 `0x030F536C=5,471`, DOS `AH=3Bh=580`, segment-store HLE `1,881`로 일치했습니다.
- frame당 CPU cycle은 119,033,852.559에서 105,048,207.445로 11.75% 감소했습니다.
- frame당 전체 예외는 860.049에서 748.715로 12.95% 감소했습니다.
- frame당 hotspot HLE outcome은 51.183에서 21.186으로 58.61%, HLE cycle은 3,457,582.884에서 1,539,138.024로 55.49% 감소했습니다.
- 평균 buffer-swap 처리율은 31.004 FPS에서 35.131 FPS로 13.31% 증가했지만 캡처 길이가 달라 보조 근거로만 판단했습니다.
- 활성 캡처의 host dispatch entry/attempt/success/fallback은 `29,952/29,952/29,936/16`이며, 16건은 기존 INT3 fallback으로 안전하게 복구되었습니다.

### 구현과 검증

- `aot-dbt`에서 `REPIU_AOT_DBT_PORT_IO_DISPATCH`가 설정되지 않으면 기본 ON이 되도록 승격했습니다.
- `1|on|true`는 명시적 ON이며 `0|off|false` 및 알 수 없는 값은 fail-closed opt-out입니다. 다른 backend에는 적용하지 않습니다.
- Release Win32 probe와 loader 빌드가 성공했습니다. 기존 C4819/LNK4217 경고만 남았습니다.
- 전체 probe는 종료 코드 0이며 `port_io_dispatch_specific=true`, 모든 기존 superblock 항목 true, `selector_guard_all=true`입니다.
- 격리 EEPROM 1초 스모크에서 미설정 기본 경로는 `Port-I/O dispatch enabled: true`, 명시적 `0` 경로는 `false`를 기록했습니다. 양쪽 모두 종료 코드 0과 정상 timeout teardown을 기록했습니다.

## English

### Capture decision

- The comparison uses the 32.835-second/1,018-frame guarded-segment-read capture and the 37.204-second/1,307-frame Port-I/O-dispatch capture.
- Fixed initialization markers matched: `0x030F536C=5,471`, DOS `AH=3Bh=580`, and segment-store HLE `1,881` on both sides.
- CPU cycles per frame fell 11.75%, from 119,033,852.559 to 105,048,207.445.
- Total exceptions per frame fell 12.95%, from 860.049 to 748.715.
- Hotspot HLE outcomes per frame fell 58.61%, from 51.183 to 21.186; HLE cycles per frame fell 55.49%, from 3,457,582.884 to 1,539,138.024.
- Average buffer-swap throughput rose 13.31%, from 31.004 to 35.131 FPS, but differing capture lengths make this supporting evidence only.
- Host dispatch entry/attempt/success/fallback in the enabled capture was `29,952/29,952/29,936/16`; the 16 cases safely recovered through the existing INT3 fallback.

### Implementation and verification

- Promoted the unset `REPIU_AOT_DBT_PORT_IO_DISPATCH` default to ON for `aot-dbt`.
- `1|on|true` explicitly enables it; `0|off|false` and unknown values are fail-closed opt-outs. Other backends are unaffected.
- Release Win32 probe and loader builds passed with only pre-existing C4819/LNK4217 warnings.
- The full probe exited zero with `port_io_dispatch_specific=true`, all existing superblock checks true, and `selector_guard_all=true`.
- In isolated-EEPROM one-second smokes, the unset default logged `Port-I/O dispatch enabled: true` and explicit `0` logged `false`. Both exited zero through normal timeout teardown.
