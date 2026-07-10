# INT 21h AH=47h 현재 디렉터리 조회 설계

## 배경

`piu_1st`는 interrupt 09h vector 조회/설정 흐름을 통과한 뒤 relocated base + `0x000F4221`의 `CD 21`에서 중단된다. 당시 `EAX=0x00004700`이므로 요청은 DOS `INT 21h AH=47h`, 현재 디렉터리 조회이다.

기존 HLE는 `AH=3Bh` current directory 변경과 `AH=3Dh` file open에서 `DosVirtualFileSystemState`의 가상 current directory를 이미 사용한다. 따라서 `AH=47h`도 같은 상태를 조회해야 한다.

## 설계

공용 DOS virtual filesystem 계층에 현재 디렉터리 문자열 생성 helper를 추가한다.

* `DosVirtualFileSystemState::current_components`를 DOS path 문자열로 변환한다.
* DOS `AH=47h` 반환 형식에 맞춰 drive와 선행 `\`는 포함하지 않는다.
* root directory는 빈 문자열로 반환한다.
* Win32 execution trampoline은 `DS:SI` 버퍼에 ASCIZ 문자열을 기록하고 carry flag를 clear한다.
* 현재 구현은 drive별 current directory table을 도입하지 않고, `DL=0` 또는 기본 drive 요청으로 처리한다.
* 버퍼 쓰기가 guest runtime memory 밖이면 DOS path-not-found 오류로 실패 처리한다.

## 기대 결과

`piu_1st`가 `AH=47h` 지점을 통과하고 다음 HLE 요구사항을 드러내야 한다. 현재 `\DATAS\BGA` 상태에서는 guest buffer에 `DATAS\BGA`가 기록되어야 한다.

## 범위 밖

* drive별 current directory 상태
* host process current directory 변경
* root asset fallback 정책

# INT 21h AH=47h Get Current Directory Design

## Background

After passing the interrupt 09h vector query/set flow, `piu_1st` stops at `CD 21` at relocated base + `0x000F4221`. `EAX=0x00004700`, so the request is DOS `INT 21h AH=47h`, get current directory.

The existing HLE already uses `DosVirtualFileSystemState` for `AH=3Bh` current directory changes and `AH=3Dh` file opens. `AH=47h` should therefore query the same state.

## Design

Add a current-directory string helper to the shared DOS virtual filesystem layer.

* Convert `DosVirtualFileSystemState::current_components` into a DOS path string.
* Match the DOS `AH=47h` return format by excluding the drive and leading `\`.
* Return an empty string for the root directory.
* The Win32 execution trampoline writes the ASCIZ string to the `DS:SI` buffer and clears carry flag.
* The current implementation does not introduce per-drive current directory tables, and treats `DL=0` or default-drive requests through the single virtual current directory.
* If the guest buffer is outside runtime memory, fail with DOS path-not-found.

## Expected Result

`piu_1st` should pass the `AH=47h` point and reveal the next HLE requirement. With the current `\DATAS\BGA` state, the guest buffer should receive `DATAS\BGA`.

## Out Of Scope

* Per-drive current directory state
* Changing the host process current directory
* Root asset fallback policy
