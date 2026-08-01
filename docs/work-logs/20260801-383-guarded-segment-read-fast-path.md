# 20260801-383 Guarded Segment Read Fast-Path 작업 로그 / Work Log

설계: [20260801-383-guarded-segment-read-fast-path.md](../design/20260801-383-guarded-segment-read-fast-path.md)
작업 지시: [20260801-383-guarded-segment-read-fast-path.md](../work-orders/20260801-383-guarded-segment-read-fast-path.md)

## 한국어

### 구현

- `mov r32, Sreg` register form을 `kGuardedSegmentRead`로 분류했습니다.
- generated slot이 EFLAGS와 EAX를 보존한 상태에서 실제 CPU selector와 shadow selector를 비교하도록 했습니다. 같은 경우에만 대상 GPR의 하위 16비트를 갱신하고, 다르면 상태를 복구한 뒤 기존 INT3 HLE boundary로 돌아갑니다.
- static placement, dynamic append, selector re-resolution이 guard와 load에 쓰이는 두 shadow 주소 및 fallback 위치를 유지하도록 했습니다.
- `REPIU_AOT_GUARDED_SEGMENT_READ=1` opt-in을 추가했으며 기본값은 off입니다.
- selector guard probe에 분류, 31-byte layout, patch, fallback, 기본 비활성화 검증을 추가했습니다.
- 사용자 지시에 따라 DOS `AH=3Bh` chdir 최적화는 후순위로 남겼습니다.

### 구현 중 발견한 회귀와 수정

첫 구현은 shadow selector를 직접 zero-extend했고, 다음 구현은 상위 16비트를 보존했지만 둘 다 동일한 `0x03042EBE` 실행 회귀를 만들었습니다. 원인은 기존 HLE의 `ReadGuestSegmentSelector`가 물리 selector와 shadow가 다를 때 별도 복구 정책을 적용한다는 점이었습니다. 최종 구현은 둘이 같은 경우만 native로 처리하고 모든 divergence를 HLE로 보내도록 범위를 좁혔습니다.

### 검증 결과

- `repiu_aot_probe` Release Win32 빌드: 성공. 기존 C4819/LNK4217 경고만 남았습니다.
- 전체 probe: 종료 코드 0, `guarded_segment_read_ready/layout/patch/disabled_falls_back=true`, `selector_guard_all=true`.
- `repiu_loader_win32` Release Win32 빌드: 성공. 기존 C4819 경고만 남았습니다.
- 동일한 루트 `eeprom.dat`의 개별 복사본을 사용한 5초 A/B:
  - off: 정상 timeout, 전체 예외 81,108회, segment store HLE 15,260회.
  - on: 정상 timeout, 54개 site 활성, 전체 예외 68,386회, segment store HLE 1,818회.
- 짧은 스모크에서 전체 예외는 15.7%, segment store HLE는 88.1% 감소했습니다. 실행 구간 변동이 있으므로 이 수치는 기능 안전성 신호이며 최종 Music Select 성능 결론은 아닙니다.

## English

### Implementation

- Classified register-form `mov r32, Sreg` as `kGuardedSegmentRead`.
- The generated slot preserves EFLAGS and EAX while comparing the physical CPU selector with the shadow selector. It updates the destination GPR's low 16 bits only on equality; on mismatch it restores state and returns to the existing INT3 HLE boundary.
- Static placement, dynamic append, and selector re-resolution retain both shadow-address patches and fallback location.
- Added the `REPIU_AOT_GUARDED_SEGMENT_READ=1` opt-in, off by default.
- Extended the selector guard probe with classification, 31-byte layout, patch, fallback, and default-disable checks.
- Per user direction, DOS `AH=3Bh` chdir optimization remains lower priority.

### Regression found and corrected during implementation

The first implementation directly zero-extended the shadow selector. A second version preserved the upper 16 bits, but both produced the same execution regression at `0x03042EBE`. Existing HLE `ReadGuestSegmentSelector` applies recovery policy when the physical selector diverges from the shadow. The final implementation narrows native execution to equality and sends every divergence to HLE.

### Verification results

- Release Win32 `repiu_aot_probe` build: passed, with only pre-existing C4819/LNK4217 warnings.
- Full probe: exit code 0; `guarded_segment_read_ready/layout/patch/disabled_falls_back=true` and `selector_guard_all=true`.
- Release Win32 `repiu_loader_win32` build: passed, with only pre-existing C4819 warnings.
- Five-second A/B using separate copies of the same root `eeprom.dat`:
  - off: normal timeout, 81,108 total exceptions, 15,260 segment-store HLE calls.
  - on: normal timeout, 54 enabled sites, 68,386 total exceptions, 1,818 segment-store HLE calls.
- The short smoke reduced total exceptions by 15.7% and segment-store HLE calls by 88.1%. Workload timing can vary, so this is a safety signal, not the final Music Select performance conclusion.
