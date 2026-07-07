# 실행 파일 설계 노트

## 목적

이 문서는 원본 실행 파일 분석으로 확인한 구조와 로더 설계 결정을 누적 기록한다.

## 현재 확인 사항

`MASTER\PIU_1ST\PIU.EXE`에 대해 현재 확인한 내용:

* 파일은 `MZ` DOS 헤더로 시작한다.
* `MZ` 헤더의 `e_lfanew` 값은 `0x2C90`이다.
* `0x2C90` 위치에는 `LE` 시그니처가 있다.
* 같은 디렉터리에 `DOS4GW.EXE`가 존재하지만, 프로젝트 방향은 DOS4GW를 외부 런타임으로 실행하는 것이 아니라 LE 이미지를 직접 분석하고 필요한 DOS/DPMI 서비스를 HLE로 제공하는 것이다.

## 설계 방향

* 실행 파일 분석은 MZ 파서와 LE 파서로 분리한다.
* 버전별 실행 경로와 자산 루트는 타깃 프로파일로 관리한다.
* 첫 실행 전에는 비실행 분석 도구로 엔트리 포인트, 오브젝트 테이블, 페이지 테이블, fixup 정보를 확인한다.

## 비실행 분석 도구

첫 C++ 도구는 `PIU.EXE`를 실행하지 않고 읽어서 MZ/LE 고정 헤더 정보를 출력한다.

초기 LE 파서는 다음 고정 필드를 해석한다.

* byte order
* word order
* CPU type
* OS type
* module flags
* module page count
* entry object/index
* entry offset
* stack object/index
* stack offset
* page size
* object table offset/count
* object page table offset
* fixup page table offset
* fixup record table offset
* data pages offset

오브젝트 테이블, 페이지 테이블, fixup record의 상세 파싱은 다음 단계에서 누적한다.

## LE 이미지 매핑 Dry-Run

현재 확인한 `PIU.EXE`의 LE 오브젝트 테이블은 24바이트 레코드 4개로 구성된다.

페이지 테이블은 4바이트 레코드이며, 앞 3바이트는 big-endian 형태의 data page 번호, 마지막 1바이트는 page flags로 해석한다.

`data_pages_offset`는 파일 절대 오프셋으로 사용한다. data page 번호 1은 `data_pages_offset`, 번호 N은 `data_pages_offset + (N - 1) * page_size`를 가리킨다.

현재 dry-run 매핑은 각 오브젝트의 `virtual_size`만큼 버퍼를 만들고, 오브젝트가 참조하는 페이지를 해당 버퍼에 복사한다. 마지막 페이지와 오브젝트 끝은 가상 크기를 넘지 않도록 잘라서 복사한다.

이 단계에서는 fixup record를 해석하거나 relocation을 적용하지 않는다.

## LE Fixup Section 분석

LE fixup page table은 `page_count + 1`개의 32-bit little-endian offset으로 해석한다.

각 offset은 fixup record table 시작 기준 상대 offset이다.

page N의 fixup record 범위는 `offset[N]`부터 `offset[N + 1]` 직전까지이다.

현재 단계에서는 이 범위가 단조 증가하는지, fixup record table 크기 안에 들어오는지, page별 fixup span이 얼마나 되는지만 검증한다.

fixup record의 가변 길이 구조와 relocation 적용은 다음 단계에서 별도 설계로 진행한다.

## LE Fixup Record 1차 디코딩

fixup record는 가변 길이이므로, 현재 단계에서는 `PIU.EXE`에서 관찰되는 내부 참조 형태를 우선 지원한다.

공통 record 시작 구조는 `source_type`, `target_flags`, 16-bit `source_offset`으로 해석한다.

내부 target은 1바이트 object 번호와 target offset으로 해석한다. target offset 크기는 `target_flags`의 32-bit offset flag 여부에 따라 16-bit 또는 32-bit로 처리한다.

지원하지 않는 flag 조합은 relocation 실패로 처리하지 않고 unsupported record로 집계한다.

## 내부 Relocation Dry-Run

현재 `PIU.EXE`의 fixup record는 모두 내부 target으로 디코딩된다.

내부 relocation 값은 target object의 `relocation_base_address`와 `target_offset`을 더한 값으로 계산한다.

source 위치는 fixup record의 page index와 source offset을 통해 해당 오브젝트 버퍼 내 offset으로 변환한다.

현재 관찰된 source kind `0x07`은 32-bit little-endian write로 처리한다. 다른 source kind는 selector/포인터 의미가 더 필요하므로 이 단계에서는 skipped로 집계한다.

일부 record는 현재 4KB page buffer 모델에서 직접 write할 수 없는 source offset을 사용하므로 source out-of-range skipped로 집계한다.

## Relocation Skipped Source 분석

skipped relocation은 아직 의미를 확정하지 않고, source kind별 count와 첫 sample을 출력해 다음 단계 설계 근거로 사용한다.

현재 출력해야 하는 sample은 첫 unsupported source kind record와 첫 source out-of-range record이다.

source kind만으로는 상위 source flag 의미를 구분할 수 없으므로 full source type별 count도 함께 출력한다.

unsupported source는 kind별 첫 sample을 출력해 `source_type=0x13`과 순수 `source kind 0x05` 같은 사례를 분리한다.

이 단계는 실행 가능한 메모리에 쓰지 않고, 분석용 LE 이미지 버퍼에만 값을 적용한다.

## DOS/4GW Loader Result

분석 도구와 향후 runtime이 같은 로딩 흐름을 사용하도록 `Dos4gwLoadResult`를 추가한다.

`Dos4gwLoadResult`는 MZ 헤더, LE 헤더, 매핑된 LE 이미지, fixup section 분석 결과, fixup record 디코딩 결과, relocation dry-run 결과를 하나로 묶는다.

`LoadDos4gwExecutable`은 target profile의 format hint가 `DOS4GW_LE`인지 확인한 뒤 기존 MZ/LE/image/fixup/relocation 순서로 결과를 채운다.

## Runtime Memory Dry-Run

`Dos4gwLoadResult`를 입력으로 받아 실행 전 runtime memory 배치 계획을 계산한다.

현재 dry-run은 LE object의 relocation base address와 virtual size를 runtime object region으로 기록한다.

entry linear address는 entry object base와 entry offset을 더해 계산한다.

stack top linear address는 stack object base와 stack offset을 더해 계산한다. stack offset은 object 끝을 가리킬 수 있으므로 object size와 같은 값도 유효하게 본다.

HLE reserve base는 모든 object region 끝의 최댓값을 4KB 단위로 올림 정렬해 계산한다.

## Win32/x86 Runtime Memory Policy

Win32/x86 실행 정책은 runtime memory dry-run 결과를 기반으로 직접 실행 가능성과 필요한 예약 주소 범위를 보고한다.

원본 32-bit x86 entry로 직접 제어를 넘기는 방식은 32-bit host process에서만 지원한다.

64-bit host process에서는 직접 entry 호출을 unsupported로 보고하며, 향후 32-bit helper process 또는 별도 execution backend가 필요하다.

preferred allocation base는 runtime object region 중 가장 낮은 base address이다.

required reserve size는 HLE reserve base에서 preferred allocation base를 뺀 값이다.
## Win32 주소 범위 Dry-Run

Win32 runtime memory policy가 계산한 `preferred_allocation_base`와 `required_reserve_size`를 바탕으로, 실제 메모리 예약 전에 현재 프로세스 주소 공간을 검사한다.

이번 단계는 `VirtualAlloc`을 호출하지 않는다. Win32 전용 `ProbeWin32RuntimeAddressRange` 함수가 `VirtualQuery`로 `[preferred_allocation_base, hle_reserve_base)` 범위를 순회하고, 모든 region이 `MEM_FREE`인지 확인한다.

범위 안에서 `MEM_RESERVE` 또는 `MEM_COMMIT` 등 비어 있지 않은 region이 발견되면 analyzer는 실패하지 않고 첫 blocking block의 base, size, state를 출력한다.

이 결과는 이후 실제 executable memory allocation 정책을 정할 때 사용한다. 특히 목표 주소 범위가 이미 점유된 경우, 32-bit helper process 초기화 순서 조정 또는 relocation fallback 필요 여부를 판단하는 근거가 된다.
# Win32 Relocated Image Placement

Win32 relocated image placement는 relocated image buffer를 실제 Win32 process memory에 배치한다.

기존 Win32 host image base `0x01000000`은 relocated image base와 충돌하므로, Win32 x86 host image base는 `0x10000000`으로 이동한다.

relocated image는 `0x01000000`에 `VirtualAlloc(MEM_RESERVE | MEM_COMMIT)`으로 확보하고, object별 buffer를 해당 주소로 복사한다.

복사 후 object flags를 기준으로 `VirtualProtect`를 적용한다. 현재 최소 정책은 writable bit `0x2`, executable bit `0x4`를 기준으로 `PAGE_READWRITE`, `PAGE_EXECUTE_READ`, `PAGE_EXECUTE_READWRITE`, `PAGE_READONLY` 중 하나를 선택한다.

이번 단계는 원본 entry를 호출하지 않는다. 목표는 relocated image가 Win32 x86 process memory 안에 실행 준비 형태로 배치될 수 있는지 확인하는 것이다.

# Relocated Image Buffer

Relocated image buffer는 relocatable runtime image plan을 실제 C++ owned buffer로 구체화한다.

각 LE object buffer는 기존 mapped object memory에서 복사한다. 그 뒤 fixup record를 다시 순회하면서 source kind `0x07` record에 대해 relocated target address를 source 위치에 32-bit little-endian 값으로 기록한다.

첫 적용 sample은 기존 original relocation 값 `0x002A4B3D`가 relocated 값 `0x01294B3D`로 교체되는지 확인하는 기준으로 사용한다.

이번 단계는 아직 `VirtualAlloc` executable memory를 사용하지 않으며, 원본 entry도 호출하지 않는다.

# Relocatable Runtime Image Dry-Run

Relocatable runtime image dry-run은 원본 LE object의 상대 배치를 유지하면서 전체 image를 새 base로 이동하는 계획을 계산한다.

현재 기본 relocated image base는 `0x01000000`이다.

원본 image base는 가장 낮은 LE object base인 `0x00010000`으로 보고, relocation delta는 `0x00FF0000`으로 계산한다.

각 object의 새 base는 `original_object_base + delta`로 계산한다. 이 방식은 object 사이 간격과 object 내부 offset을 유지하면서 Windows 32-bit 낮은 주소 충돌을 피하기 위한 것이다.

entry와 stack top도 같은 object index와 offset을 사용해 새 object base 기준으로 다시 계산한다.

relocation dry-run은 source kind `0x07` record를 32-bit internal pointer write로 보고, target object의 relocated base와 target offset을 더한 값을 새 적용 값으로 계산한다. 나머지 source kind와 source out-of-range record는 기존과 같이 skipped로 남겨 위험을 추적한다.

# Relocation 기반 로드 결정

Win32 x86 프로세스에서 원본 DOS/4GW 이미지가 기대하는 낮은 주소 범위를 그대로 예약하는 방식은 안정적인 기본 경로로 보기 어렵다.

따라서 다음 단계부터는 원본 LE relocation 정보를 사용해 안전한 새 runtime base에 이미지를 올리는 방식을 우선 설계한다.

원본 주소 고정 로드는 비교와 검증용 fallback으로 남기며, 실행 주 경로는 relocatable runtime image로 이동한다.

이 방식은 원본 게임 로직을 재작성하는 것이 아니다. 원본 32-bit x86 코드는 그대로 실행 대상으로 유지하고, loader가 원본 relocation metadata를 적용해 주소 배치만 바꾼다.

# Win32 Execution Host 초기 예약

`TargetProfile`에 `TargetRuntimeReservationHint`를 추가하여 target별 초기 runtime 예약 범위를 보관한다.

현재 `piu_1st`는 이전 runtime memory dry-run 결과를 바탕으로 `base=0x00010000`, `size=0x005D7000`을 예약 힌트로 가진다.

`repiu_win32_execution_host`는 실행 파일을 읽거나 LE image를 복사하기 전에 이 힌트를 사용해 Win32 fixed range policy를 만들고, `VirtualAlloc(MEM_RESERVE)`로 목표 범위 예약을 시도한다.

이번 단계는 예약 성공 여부만 관찰한다. 원본 image copy, page commit/protection, HLE dispatcher, 원본 entry 호출은 이후 단계에서 추가한다.

# Win32 Host Image Base 정책

Win32 x86 host executable이 원본 DOS/4GW 이미지의 고정 주소 범위와 직접 충돌하지 않도록 CMake에 `repiu_configure_win32_execution_host` 정책 함수를 추가한다.

현재 `PIU.EXE` runtime memory dry-run의 HLE reserve base는 `0x005E7000`이므로, Win32 x86 host image base는 이보다 높은 `0x01000000`으로 지정한다.

MSVC 32-bit 빌드에서는 `/BASE:0x01000000`과 `/DYNAMICBASE:NO`를 적용한다. 현재는 dedicated execution host가 없으므로 `repiu_exe_analyzer`에 먼저 적용하고, 이후 실행 전용 host target이 생기면 같은 정책을 재사용한다.

이 정책은 실제 메모리 예약을 수행하지 않는다. host executable 자체가 낮은 주소 범위를 차지하는 위험을 줄이고, 이후 `VirtualAlloc` 기반 예약 단계의 전제 조건을 정리하기 위한 것이다.
