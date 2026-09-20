# Task 727 작업 지시: Linux x64 LFB native-store source census

## 한국어

### 작업 항목

1. 별도 opt-in telemetry 상태에 고정 크기 guest-EIP source table과 overflow aggregate를 추가합니다.
2. positive LFB overlap에서만 EIP table을 갱신하도록 observer/profile API를 확장합니다.
3. snapshot과 종료 report에 결정적 top-8 ranking을 추가합니다.
4. 기존 Windows x86 synthetic probe에 aggregate, overflow, tie-break 검증을 추가합니다.
5. Linux x64 및 Win32 x86 Debug 검증과 Linux bounded 관찰을 수행합니다.
6. architecture, analysis, work log를 갱신하고 한 작업 커밋으로 남깁니다.

### 완료 조건

- default setting에서 observer emission, execution-context layout, LFB 의미가 변하지 않습니다.
- source table은 매 4,096번째 overlap만 기록하고 hot path에서 동적 할당이나 정렬을 하지 않습니다.
- probe가 aggregation, capacity overflow, deterministic ranking을 검증합니다.
- Linux run의 source ranking과 evidence boundary가 문서화됩니다.

### 범위 제외

- LFB readback/decode/present 최적화
- EIP를 원본 함수 또는 draw ownership으로 단정
- implicit/string/segment store의 완전한 추적

---

## English

### Work items

1. Add a fixed guest-EIP source table and overflow aggregate to separate opt-in telemetry state.
2. Extend the observer/profile API so only positive LFB overlap updates the table.
3. Add deterministic top-eight ranking to snapshot and shutdown reporting.
4. Extend the existing Windows x86 synthetic probe for aggregation, overflow, and tie-breaks.
5. Perform Linux x64 and Win32 x86 Debug checks plus a bounded Linux observation.
6. Update architecture, analysis, and work log, then create one task commit.

### Completion criteria

- Default settings leave observer emission, execution-context layout, and LFB semantics unchanged. The source setting alone enables its required observer and active-range gate.
- The source table records only every 4,096th overlap and performs no dynamic allocation or sorting in the hot path.
- The probe verifies aggregation, capacity overflow, and deterministic ranking.
- Linux source ranking and evidence boundary are documented.

### Out of scope

- LFB readback/decode/present optimization
- Declaring an EIP to be an original function or draw owner
- Complete tracing of implicit/string/segment stores
