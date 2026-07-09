# DOS Current Directory HLE 작업 로그

## 결과

공용 HLE 계층에 `DosVirtualFileSystemState`와 DOS path resolver를 추가했다. Target profile의 `working_directory`를 가상 DOS 드라이브 루트로 사용하고, host process current directory는 변경하지 않는다.

Win32 execution trampoline은 `INT 21h AH=0x3B`에서 `EDX`가 가리키는 ASCIZ path를 읽고 가상 current directory를 갱신한다. `piu_1st`에서는 `\datas\bga`가 `\DATAS\BGA`로 해석되었고, host 경로 `MASTER\PIU_1ST\DATAS\BGA`가 존재해 성공 처리되었다.

현재 다음 중단 지점은 `0x020F740C`의 `INT 21h AH=0x3D` DOS open 요청이다.

## 검증

다음 검증을 완료했다.

* `scripts\test_all.ps1`: 통과. `dos4gw_hello`는 계속 `Hello, world!`를 출력했고, `piu_1st`는 `INT 21h AH=0x3B`을 성공 처리한 뒤 `0x020F740C`의 `INT 21h AH=0x3D` 지점까지 진행했다.
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`: 통과. 전체 819개 중 overall pass 419개, overall pass rate 51.2%, regression 0개, new pass 0개다.

# DOS Current Directory HLE Work Log

## Result

Added `DosVirtualFileSystemState` and a DOS path resolver to the shared HLE layer. The target profile `working_directory` is used as the virtual DOS drive root, and the host process current directory is not changed.

The Win32 execution trampoline now reads the ASCIZ path pointed to by `EDX` for `INT 21h AH=0x3B` and updates the virtual current directory. In `piu_1st`, `\datas\bga` resolved to `\DATAS\BGA`, and the host path under `MASTER\PIU_1ST\DATAS\BGA` existed, so the request succeeded.

The current next stop is the DOS open request `INT 21h AH=0x3D` at `0x020F740C`.

## Verification

Completed the following verification:

* `scripts\test_all.ps1`: passed. `dos4gw_hello` still printed `Hello, world!`, and `piu_1st` handled `INT 21h AH=0x3B` successfully before advancing to the `INT 21h AH=0x3D` stop at `0x020F740C`.
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`: passed. Overall pass is 419 of 819, overall pass rate is 51.2%, regression count is 0, and new pass count is 0.
