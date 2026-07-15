# 20260715-206-exception-diagnostic-and-buffer-provenance-log

## 1. 작업 개요 (Task Summary)
- **작업명 (Task Name):** 예외 진단 정합성 개선 및 Opcode 88 스토어 에뮬레이션 추가 (Exception Diagnostic Correction & Opcode 88 Shadow HLE)
- **목적 (Objective):** 
  - SEH 예외 시 stale `last_guest_eip` 대신 AOT 주소 매핑을 활용해 정확한 게스트 예외 유발 EIP 및 바이트 창 출력.
  - 65,536-레코드 게스트 디코드 루프 내 `mov [ebx+ebp], al` (opcode `88`) 명령이 미매핑 주소 `0x045D3EB0`에 쓰는 오버플로우 스토어를 shadow memory HLE 에뮬레이션으로 우회하도록 구현.

---

## 2. 세부 구현 및 해결 과정 (Implementation & Resolution Details)

### 1) 진단 매핑 주소 보정 (Diagnostic Guest Address Mapping)
- `src/host/win32/main.cpp` 내의 예외 포착 진단 블록에서 AOT 주소 매핑 정보가 있는 경우 (`attempt.aot_exception_mapping_valid` 가 true인 경우) `attempt.seh_exception_address` 대신 `attempt.aot_exception_guest_address`를 사용하여 바이트 윈도우 진단을 구성하도록 수정하였습니다.
- 이로 인해 실제 예외를 발생시킨 명령어 위치가 바이트 윈도우에 정확하게 포커스되어 나타나게 되었습니다.

### 2) Opcode 88 에뮬레이션 구현 (Opcode 88 HLE Emulation)
- `src/platform/win32/execution_trampoline.cpp` 내의 `HandleTracedMemoryStoreInstruction` 함수에 `instruction[0] == 0x88` 핸들러 분기를 추가하고 `supported_boundary_store` 조건으로 등록하였습니다.
- `DecodeModRmMemoryAddress`로 타겟 오프셋을 구한 후, `ReadRegister8`을 통해 8비트 소스 레지스터 값을 추출하여 shadow memory 에뮬레이션을 성공적으로 수행하였습니다.

### 3) 가드 페이지 보호 간섭 버그 해결 (Resolving Guard Page Bypass Bug)
- **문제 발생 (Issue):** 
  - 구현 직후 검증 시 실행 2.5초 지점(telemetry heartbeat 77,155 부근)에서 프로세스가 exit code 1로 크래시되는 현상이 관측되었습니다.
  - 원인을 조사한 결과, 파일 오픈 도중의 임시 shadow write 로직(`last_dos_open_success` 관련 조건)이 개입하면서, 실제 가상 메모리 공간(정상적인 가드 페이지 영역)에 대한 스토어 작업마저 shadow memory 에뮬레이션이 가로채는 부작용이 발생했음을 파악하였습니다.
  - 이로 인해 AOT 가드 페이지 복구 핸들러로 예외가 도달하지 못해 페이지가 writable로 해제되지 않았고, 결국 `STATUS_GUARD_PAGE_VIOLATION` (`0x80000001`)로 크래시를 유발했습니다.
- **해결 방안 (Resolution):**
  - sentinel, metadata, boundary store 등 합법적인 shadow memory 대상을 제외한 모든 일반 스토어들에 대해, 만약 쓰기 주소가 relocated image 가상 메모리 범위(`runtime_base <= destination < runtime_end`) 내부라면 무조건 우회 시도를 차단하고 `false`를 리턴하도록 안전 필터를 보강하였습니다.
  - 이로써 가드 페이지 쓰기 예외는 정상적으로 AOT 복구 핸들러로 전달되어 페이지 보호 해제가 성공하였고, 프로그램은 이 크래시를 극복하고 정상 진행되었습니다.

### 4) LINEXE 고정 세그먼트 Limit 확장 도입 및 롤백 (LINEXE Segment Limit Expansion & Rollback)
- **임시 패치:** `0x030F3A21` (`mov al, gs:[ebx]`) 실행 시의 LDT limit 초과 크래시를 회피하기 위해, 로더 내 고정 세그먼트(`0x0020`, `0x0090`, `0x0080`, `0x0088`)의 LDT descriptor limit를 `4GB` (`0xFFFFFFFFU`)로 임시 강제 확장하는 패치를 도입하였습니다.
- **회귀 분석 및 롤백:** 
  - 4GB 확장 후 실행 시, 이전과 달리 OpenGL 윈도우 창이 정상 생성되지 않고 `0x030F3438` (Import stub dynamic patching 지점) 부근에서 Exception Storm (무한 루프)에 걸려 멈추는 회귀 버그가 발생했습니다.
  - 조사 결과, Task 205에서 도입된 `POP ES/FS/GS` HLE 에뮬레이션 시 실제 CPU 하드웨어의 물리 세그먼트 레지스터 값을 복원(로드)해 주지 않아 물리 GS 레지스터가 쓰레기 값으로 고착되었고, 이로 인해 GS 세그먼트 예외가 발생했음을 규명하였습니다.
  - **해결 조치:** `HandleSegmentPopInstruction` 에서 `POP ES/FS/GS` 에뮬레이션 가로채기를 전부 제거하여, 원래처럼 하드웨어 CPU가 직접 로드하도록 패스스루(Pass-through) 원복하였습니다. 이에 따라 GS 레지스터가 완벽하게 복원되어 **4GB 강제 확장 패치 또한 불필요해져 깨끗하게 롤백(제거)**하였습니다.

---

## 3. 검증 결과 및 확인된 사실 (Verification Results & Findings)

### 1) 빌드 검증 (Build Verification)
- `powershell -ExecutionPolicy Bypass -File .\scripts\build_win32_x86.ps1` 빌드가 에러 없이 정상 완료되었습니다.

### 2) 런타임 결과 (Runtime Results)
- **SMC / 레지스터 회귀 해소:** `POP GS`가 하드웨어적으로 정확하게 세그먼트를 갱신함에 따라, `0x030F3A21` (`mov al, gs:[ebx]`) 의 GS limit 위반 크래시가 완벽하게 해소되었습니다.
- **Glide HLE 진입 성공:** 게스트 스레드는 `0x030F3438` 의 예외 폭풍(Stall)을 완전히 극복하고, dynamic AOT 백엔드 상에서 Glide HLE Gate (`last_eip = 0x045D05B0` 대역)로 정상적이고 안전하게 진입하여 구동 중임을 최종 검증하였습니다.

---

## 4. 후속 작업 제안 (Follow-up Recommendation)
- 세그먼트 레지스터 가상화 오류 및 회귀 락이 완전히 해소되어, Glide API 호출 및 OpenGL 디스패치 파이프라인으로 안전하게 진입할 수 있는 환경이 복구되었습니다.
- 다음 단계에서는 기동 과정 중 Voodoo 가속 카드 검출에 필요한 IO 포트 입출력 HLE 및 관련 하드웨어 감지 인터페이스들의 세부 동작을 트래킹하고 보완하는 후속 작업을 제안합니다.
