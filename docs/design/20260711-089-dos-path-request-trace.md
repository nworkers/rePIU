# DOS path request trace 설계

## 배경

현재 `piu_1st` 로드 실패 조사에서는 `chdir`, `getcwd`, `get drive`, `open`의 마지막 요청만 로그에 남는다. 최신 실행에서는 콘솔 출력이 `FAIL: res_load( spr.res )`인데 마지막 open은 `piu.bin`으로 표시되어, 실제 파일 요청 순서를 last-only 로그만으로 판단하기 어렵다.

이번 작업은 파일 오픈 정책이나 current-directory 정책을 바꾸지 않고, 원본 코드가 요청한 디렉터리/파일 관련 DOS 호출 순서를 확인할 수 있게 한다.

## 설계

Win32 execution attempt에 최근 DOS path 요청 trace를 추가한다.

* trace capacity는 16개로 제한한다.
* 대상 서비스는 `chdir`, `getcwd`, `getdrive`, `open`이다.
* 각 entry에는 sequence, service, guest path, virtual path, host path, result, DOS error, drive, access mode를 기록한다.
* `getcwd`와 `getdrive`처럼 path가 없는 서비스도 같은 trace에 기록해 호출 순서를 보존한다.
* 기존 last-only 로그는 호환성을 위해 유지한다.

이 trace는 관측용이다. `\DATAS\BGA`에서 root asset을 fallback으로 여는 정책 변경은 포함하지 않는다.

## 기대 결과

`piu_1st` 로그에서 실제 요청 순서가 보인다. 이를 통해 `chdir/getcwd`가 요청대로 동작했는지, 파일 open 실패가 누락된 디렉터리 요청 때문인지 구분한다.

# DOS Path Request Trace Design

## Background

The current `piu_1st` load-failure investigation only logs the last request for `chdir`, `getcwd`, `get drive`, and `open`. In the latest run, the console prints `FAIL: res_load( spr.res )`, but the last open is `piu.bin`, so the true file request order cannot be inferred from last-only logs.

This task does not change file-open policy or current-directory policy. It adds visibility into the DOS directory/file request order made by the original code.

## Design

Add a recent DOS path request trace to the Win32 execution attempt.

* Limit trace capacity to 16 entries.
* Trace `chdir`, `getcwd`, `getdrive`, and `open`.
* Each entry stores sequence, service, guest path, virtual path, host path, result, DOS error, drive, and access mode.
* Path-less services such as `getcwd` and `getdrive` are recorded in the same trace so call order is preserved.
* Keep existing last-only logs for compatibility.

This trace is observational only. It does not add a policy that falls back from `\DATAS\BGA` to root assets.

## Expected Result

The `piu_1st` log shows the actual request order. This lets us distinguish whether `chdir/getcwd` behaved as requested or whether a missed directory request caused the file-open failure.
