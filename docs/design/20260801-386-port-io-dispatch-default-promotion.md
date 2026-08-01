# 20260801-386 Port-I/O Dispatch 기본 활성화 설계 / Default Promotion Design

## 한국어

### 배경과 판단

Task 385는 Port-I/O 전용 DBT host dispatch를 opt-in으로 도입했습니다. 후속 Music Select 캡처는 기존 guarded segment-read 캡처와 같은 고정 초기화 표식(`0x030F536C=5,471`, DOS `AH=3Bh=580`, segment-store HLE `1,881`)을 유지하면서, frame당 CPU cycle 11.75%, 전체 예외 12.95%, hotspot HLE outcome 58.61%, hotspot HLE cycle 55.49%를 줄였습니다. 평균 buffer-swap 처리율도 약 31.00 FPS에서 35.13 FPS로 증가했지만 캡처 길이가 다르므로 보조 근거로만 사용합니다.

### 정책

`aot-dbt` backend에서는 `REPIU_AOT_DBT_PORT_IO_DISPATCH`가 설정되지 않았을 때 전용 dispatch를 기본 활성화합니다. `1|on|true`는 명시적 활성화이고, `0|off|false` 및 알 수 없는 값은 회귀 진단을 위한 fail-closed opt-out입니다. 다른 backend에는 적용하지 않습니다.

Port-I/O 의미론, emulator, dispatch thunk 및 기존 provenance-aware INT3 fallback은 변경하지 않습니다. 이번 작업은 검증된 정책의 기본값만 승격합니다.

### 검증

Release Win32 loader와 전체 AOT probe를 빌드·실행합니다. 격리 EEPROM으로 환경변수 미설정 기본 경로와 `0` opt-out 경로가 각각 상태 로그에서 true/false인지 확인하고, 두 경로가 정상 실행되는지 확인합니다.

## English

### Background and decision

Task 385 introduced Port-I/O-specific DBT host dispatch as an opt-in. The follow-up Music Select capture preserved the same fixed initialization markers as the guarded-segment-read capture (`0x030F536C=5,471`, DOS `AH=3Bh=580`, segment-store HLE `1,881`) while reducing per-frame CPU cycles by 11.75%, total exceptions by 12.95%, hotspot HLE outcomes by 58.61%, and hotspot HLE cycles by 55.49%. Average buffer-swap throughput also rose from about 31.00 FPS to 35.13 FPS, but this is only supporting evidence because capture durations differ.

### Policy

For the `aot-dbt` backend, enable the dedicated dispatch by default when `REPIU_AOT_DBT_PORT_IO_DISPATCH` is unset. `1|on|true` explicitly enables it; `0|off|false` and unknown values are fail-closed opt-outs for regression diagnosis. Other backends remain unaffected.

Do not change Port-I/O semantics, the emulator, dispatch thunk, or the existing provenance-aware INT3 fallback. This task only promotes the verified policy default.

### Verification

Build the Release Win32 loader and full AOT probe. With isolated EEPROM copies, verify that the unset default and `0` opt-out report true and false respectively in status logs, and that both paths run normally.
