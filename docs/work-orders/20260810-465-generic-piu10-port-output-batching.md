# 20260810-465 공용 PIU10 port-output batching 작업 지시 / Generic PIU10 Port-Output Batching Work Order

설계: [20260810-465-generic-piu10-port-output-batching.md](../design/20260810-465-generic-piu10-port-output-batching.md)

## 한국어

- [x] `pumpito` 전용 gate와 고정 주소 의존성을 식별합니다.
- [x] guest instruction operand에서 상태와 경계를 추출하는 공용 설계를 작성합니다.
- [x] target 이름, EIP, object와 data offset 상수를 제거합니다.
- [x] PIU10 capability 전체에서 batch와 진단을 사용할 수 있게 합니다.
- [x] relocation-independent synthetic probe와 fail-closed probe를 추가합니다.
- [x] Win32 x86 Debug build와 PIU10 probe를 수행합니다.
- [x] architecture, 누적 분석과 작업 로그를 갱신합니다.
- [x] 변경을 커밋합니다.
- [x] 사용자 로그로 `pumpitc`의 실제 batch 활성화를 재검증합니다.
- [x] `pumpite`의 동등한 register/order 변형이 기존 shape 비교에서 거부됨을 확인합니다.
- [x] 임시 register와 독립 명령 순서에 무관한 의미 기반 matcher로 확장합니다.
- [x] 기존 schedule과 `pumpite` schedule을 각각 synthetic probe로 검증합니다.
- [x] Win32 x86 Debug build와 PIU10 probe를 다시 수행합니다.
- [x] architecture, 누적 분석과 작업 로그를 갱신합니다.
- [x] 변경을 커밋합니다.
- [x] 사용자 환경에서 `pumpite`의 실제 batch 활성화와 게임 진행 개선을 재검증합니다.

## English

- [x] Identify the `pumpito`-only gate and fixed-address dependencies.
- [x] Design generic state and boundary extraction from guest instruction operands.
- [x] Remove target-name, EIP, object, and data-offset constants.
- [x] Enable batching and diagnostics for every PIU10 capability.
- [x] Add relocation-independent and fail-closed synthetic probes.
- [x] Run Win32 x86 Debug builds and the PIU10 probe.
- [x] Update architecture, cumulative analysis, and the work log.
- [x] Commit the changes.
- [x] Revalidate live `pumpitc` batching from the user's log.
- [x] Confirm that the equivalent `pumpite` register/order form is rejected by the former shape
  comparison.
- [x] Extend the matcher to semantic validation independent of temporary registers and independent
  instruction ordering.
- [x] Probe both the original and `pumpite` schedules synthetically.
- [x] Rerun the Win32 x86 Debug builds and PIU10 probe.
- [x] Update architecture, cumulative analysis, and the work log.
- [x] Commit the change.
- [x] Revalidate live `pumpite` batch activation and improved game progress in the user's
  environment.
