# Timeout DOS observation copy 작업 로그

## 변경 내용

`ThreadContext`의 관측 상태를 `Win32MinimalExecutionAttempt`로 복사하는 `CopyThreadObservationToAttempt` helper를 추가했다. timeout 경로와 정상 종료 경로가 같은 helper를 사용하므로, timeout 결과에서도 chdir/open/ioctl/resize 세부 로그가 출력된다.

`scripts/test_all.ps1`의 `piu_1st` 기준을 갱신해 chdir 도달과 성공 결과를 다시 확인하도록 했다.

## 결과

`piu_1st`는 현재 timeout으로 끝나지만, chdir는 다시 로그에 보인다.

* `Win32 handled DOS chdir count: 1`
* guest path: `\datas\bga`
* virtual path: `\DATAS\BGA`
* host path: `MASTER\PIU_1ST\DATAS\BGA`
* result: success

그 뒤 open 관측도 출력된다. 현재 마지막 open은 `spr.res`이며, `\DATAS\BGA\SPR.RES`가 현재 자산 트리에 없어 DOS error `0x0002`로 실패한다.

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 결과: 성공

# Timeout DOS Observation Copy Work Log

## Changes

Added `CopyThreadObservationToAttempt`, a helper that copies observation state from `ThreadContext` into `Win32MinimalExecutionAttempt`. Timeout and normal completion paths now use the same helper, so timeout results include chdir/open/ioctl/resize details.

Updated the `piu_1st` expectations in `scripts/test_all.ps1` to verify that chdir is reached and succeeds again.

## Result

`piu_1st` still ends through the current timeout path, but chdir is visible in the log again.

* `Win32 handled DOS chdir count: 1`
* guest path: `\datas\bga`
* virtual path: `\DATAS\BGA`
* host path: `MASTER\PIU_1ST\DATAS\BGA`
* result: success

The following open observation is also printed. The last open is currently `spr.res`, and `\DATAS\BGA\SPR.RES` is not present in the current asset tree, so it fails with DOS error `0x0002`.

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Result: passed
