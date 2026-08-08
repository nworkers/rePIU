# 20260809-451 PIU10 ISA 보드 HLE 작업 지시 / PIU10 ISA Board HLE Work Order

설계: [20260809-451-piu10-isa-board-hle.md](../design/20260809-451-piu10-isa-board-hle.md)

## 한국어

### 목표

원본 `pumpito` 실행 파일이 사용하는 별도 PIU10 ISA 보드의 `0x02D0..0x02DF` 계약과
CAT702 보안 직렬 프로토콜을 HLE하여 `0x02DA` 입력 blocker를 제거합니다.

### 작업 항목

- [x] 플랫폼 공용 CAT702 PIU 상태 모델과 단위 probe를 추가합니다.
- [x] 플랫폼 공용 PIU10 ISA 주소, 목적지, flash, 상태 register 모델을 추가합니다.
- [x] 현재 target ROM ZIP에서 `piu10.u8`과 `<target>.cat702`를 읽어 장치를 초기화합니다.
- [x] Win32 port I/O adapter가 `0x02D0..0x02DF` 16-bit 접근을 장치에 전달하도록 연결합니다.
- [x] architecture와 누적 PIU port 분석 문서를 갱신합니다.
- [x] 전체 Win32 x86 Debug 빌드, probe, `pumpito` 실행을 검증합니다.
- [x] 작업 로그를 작성하고 하나의 Git commit으로 남깁니다.

## English

### Objective

HLE the separate PIU10 ISA board's `0x02D0..0x02DF` contract and CAT702 serial security
protocol used by the original `pumpito` executable, removing the `0x02DA` input blocker.

### Work Items

- [x] Add platform-neutral CAT702 PIU state and a unit probe.
- [x] Add the platform-neutral PIU10 ISA address, destination, flash, and status-register model.
- [x] Initialize the device from `piu10.u8` and `<target>.cat702` in the current target ROM ZIP.
- [x] Connect Win32 port I/O adapter 16-bit `0x02D0..0x02DF` accesses to the device.
- [x] Update architecture and cumulative PIU port analysis documentation.
- [x] Verify the full Win32 x86 Debug build, probes, and a `pumpito` run.
- [x] Write the work log and leave one Git commit.
