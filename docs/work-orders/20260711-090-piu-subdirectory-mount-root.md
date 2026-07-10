# 작업 지시: PIU 하위 디렉터리 실행과 마운트 루트 분리

## 목표

`piu_1st` target이 `MASTER\PIU_1ST`를 DOS 드라이브 루트로 마운트하고, `MASTER\PIU_1ST\PIU`를 초기 실행 current directory로 사용하도록 수정한다.

## 작업 항목

* `piu_1st` target profile의 executable path를 `MASTER/PIU_1ST/PIU/PIU.EXE`로 갱신한다.
* `piu_1st` target profile의 `working_directory`를 `MASTER/PIU_1ST/PIU`, `asset_root`를 `MASTER/PIU_1ST`로 둔다.
* DOS VFS 초기화가 mount root와 initial current directory를 분리해서 받을 수 있게 한다.
* Win32 host가 `asset_root`로 VFS를 마운트하고 `working_directory`를 초기 cwd로 넘기게 한다.
* 테스트 환경과 회귀 테스트의 `piu_1st` 경로/로그 기대값을 갱신한다.

## 검증

`scripts\test_all.ps1 -SkipSetup`을 실행한다. 테스트 로그에서 executable path, VFS root, VFS current directory, DOS path trace가 새 배치와 일치하는지 확인한다.

# Work order: PIU subdirectory execution and mount root separation

## Goal

Update the `piu_1st` target so it mounts `MASTER\PIU_1ST` as the DOS drive root and uses `MASTER\PIU_1ST\PIU` as the initial execution current directory.

## Tasks

* Update the `piu_1st` target profile executable path to `MASTER/PIU_1ST/PIU/PIU.EXE`.
* Set the `piu_1st` profile `working_directory` to `MASTER/PIU_1ST/PIU` and `asset_root` to `MASTER/PIU_1ST`.
* Let DOS VFS initialization accept separate mount root and initial current directory paths.
* Make the Win32 host mount `asset_root` and pass `working_directory` as the initial cwd.
* Update test environment and regression expectations for the new `piu_1st` path/logs.

## Verification

Run `scripts\test_all.ps1 -SkipSetup`. Confirm that executable path, VFS root, VFS current directory, and DOS path trace match the new layout.
