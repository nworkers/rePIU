# 작업 지시: INT 21h AH=3Eh 파일 close HLE

## 목표

`piu_1st`가 `intro.ani` seek 이후 `INT 21h AH=3Eh` file close 지점에서 중단되지 않도록 DOS file close HLE를 추가한다.

## 작업 항목

* DOS VFS에 open handle 기반 close helper를 추가한다.
* Win32 execution trampoline에 `AH=3Eh` 처리 helper를 추가한다.
* 일반/traced `INT 21h` 처리기에 `AH=3Eh` case를 연결한다.
* Win32 loader 로그와 회귀 테스트에 마지막 DOS close 관측값을 추가한다.

## 검증

`scripts\test_all.ps1 -SkipSetup`으로 전체 현재 회귀를 실행한다. `piu_1st` 로그에서 `AH=3Eh` close가 성공하고 다음 HLE 요구사항으로 진행하는지 확인한다.

# Work order: INT 21h AH=3Eh file close HLE

## Goal

Add DOS file close HLE so `piu_1st` does not stop at the `INT 21h AH=3Eh` file close point after seeking `intro.ani`.

## Tasks

* Add an open-handle-based close helper to the DOS VFS.
* Add an `AH=3Eh` helper to the Win32 execution trampoline.
* Wire `AH=3Eh` into both normal and traced `INT 21h` handling.
* Add last DOS close observation fields to the Win32 loader log and regression test.

## Verification

Run the current regression with `scripts\test_all.ps1 -SkipSetup`. Confirm that the `piu_1st` log reports a successful `AH=3Eh` close and advances to the next HLE requirement.
