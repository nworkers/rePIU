# 작업 로그: 기존 계획 대비 진행 상황 기록

## 개요
기존 `docs/DOS4G_HLE_PORTING_PLAN.md`에 정의된 전체 프로젝트 계획 대비 현재까지의 달성도를 점검하고 기록함.

## Stage 1: Executable Analysis (완료)
* **목표**: DOS4G/LE 실행 파일 파싱, 메모리 이미지 빌드
* **진행**: 완료됨. `repiu_exe_analyzer` 및 로더가 LE 포맷을 완벽하게 읽고 Object Region을 매핑함.

## Stage 2: Memory System (완료)
* **목표**: Flat 32-bit 메모리, 셀렉터 추상화
* **진행**: 완료됨. Win32 Host 메모리 할당 및 `repiu::runtime::SelectorAllocator`를 통해 16-bit/32-bit 셀렉터를 구현함.

## Stage 3: Minimal HLE (진행 중)
* **목표**: 파일 입출력, 메모리 할당, 시간 등 필수 DOS INT 21h 서비스 에뮬레이션
* **진행**: 상당 부분 완료됨. `dos_int21_services.cpp`에서 파일 입출력과 메모리 관리 기본 기능이 작동 중임.

## Stage 4: DPMI (진행 중)
* **목표**: 필수 INT 31h 서비스 에뮬레이션
* **진행**: 진행 중. 메모리 할당, 디스크립터 관리 및 인터럽트 벡터 섀도잉 기능이 추가됨. (최근 타이머 인터럽트 구현으로 진전)

## Stage 5: Graphics (진행 중)
* **목표**: VGA Mode 13h 혹은 게임 특화 렌더링 에뮬레이션
* **진행**: 게임이 Glide API를 사용하므로, `linexe_glide_boundary.cpp`를 통한 Glide Gate HLE 브리지를 구축함. 현재 초기화는 통과했으며 렌더링 명령 전달 구현이 필요함.

## Stage 6: Input (일부 진행 중)
* **목표**: 키보드/마우스 입력 처리
* **진행**: 게임이 표준 PC 입력 대신 JAMMA I/O 포트를 사용함. 포트 에뮬레이션 뼈대(`port_io_emulator.cpp`)를 구축하여 JAMMA 입력 매핑을 준비 중임.

## Stage 7: Timing (완료)
* **목표**: PIT 타이머 추상화 및 Tick 카운터
* **진행**: 완료됨. `live_telemetry_snapshot.cpp`와 실행 트램펄린을 통해 18.2Hz 주기의 BIOS 타이머(`INT 8`)를 게스트로 직접 주입(Injection)하여 하드웨어 루프를 탈출하는 데 성공함.

## Stage 8: Audio (진행 중)
* **목표**: 사운드 하드웨어 에뮬레이션
* **진행**: 사운드 초기화 포트(`0x02A0`, `0x02A2`)를 인터셉트하고 에뮬레이션하기 시작함.

## 요약
현재 프로젝트는 **초기 실행 파일 로드 및 DPMI/DOS HLE 뼈대 구축(Stage 1~4)을 완료**하고, 게임의 메인 로직이 정상 동작하도록 **타이머 인터럽트를 주입(Stage 7)하여 무한 루프를 돌파**한 상태임.
다음 목표는 HLE 렌더링(Stage 5)을 실질적으로 구현하고, 게임 로직의 예외 상황(예: AOT 없는 TRAP 실행 시의 `#GP` 크래시 방지)을 보완하는 것임.
