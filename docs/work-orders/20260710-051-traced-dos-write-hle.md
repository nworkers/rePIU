# Traced DOS Write HLE 작업 지시

## 목표

`piu_1st`가 `INT 21h AH=0x40` write 요청을 통과하도록 traced DOS HLE에 콘솔 write 처리를 추가한다.

## 작업 범위

* traced DOS interrupt 처리에 `AH=0x40` 분기를 추가한다.
* `EDX`와 `ECX`로 guest 출력 버퍼와 길이를 읽는다.
* 읽은 데이터를 기존 HLE console output 버퍼에 누적한다.
* 성공 시 `AX=CX`, CF clear로 반환한다.
* `scripts\test_all.ps1`의 `piu_1st` 기대 관측 지점을 갱신한다.

## 제외 범위

* 실제 파일 handle write는 구현하지 않는다.
* short write, disk full, sharing mode 같은 상세 DOS 파일 쓰기 오류는 다루지 않는다.

## 검증

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `scripts\test_all.ps1`

# Traced DOS Write HLE Work Order

## Goal

Add console write handling to traced DOS HLE so `piu_1st` can pass its `INT 21h AH=0x40` write request.

## Scope

* Add an `AH=0x40` branch to traced DOS interrupt handling.
* Read the guest output buffer and length from `EDX` and `ECX`.
* Append the readable data to the existing HLE console output buffer.
* Return success with `AX=CX` and CF clear.
* Update the `piu_1st` expected observation point in `scripts\test_all.ps1`.

## Out of Scope

* Do not implement real file handle writes.
* Do not model detailed DOS write errors such as short writes, disk full, or sharing modes.

## Verification

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `scripts\test_all.ps1`
