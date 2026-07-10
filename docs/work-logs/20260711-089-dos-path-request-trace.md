# DOS path request trace 작업 로그

## 결과

`piu_1st`의 디렉터리/파일 관련 DOS 요청 순서를 확인하기 위해 최근 DOS path request trace를 추가했다.

* `Win32DosPathTraceEntry`와 `Win32DosPathObservation`을 추가했다.
* `Win32MinimalExecutionAttempt`와 `ThreadContext`에 DOS path trace를 추가했다.
* `chdir`, `getcwd`, `getdrive`, `open` 처리 지점에서 trace entry를 기록한다.
* Win32 loader 로그에 trace stored count, limit 여부, 각 trace entry를 출력한다.
* `scripts/test_all.ps1`에서 현재 `piu_1st`의 path request 순서를 검증한다.

## 관측

현재 `piu_1st` 실행에서 관측된 DOS path 요청 순서는 다음과 같다.

1. `chdir` success: guest `\datas\bga`, virtual `\DATAS\BGA`
2. `open` failure `0x0002`: guest `intro.ani`, virtual `\DATAS\BGA\INTRO.ANI`
3. `open` failure `0x0002`: guest `stage.cfg`, virtual `\DATAS\BGA\STAGE.CFG`
4. `open` failure `0x0002`: guest `spr.res`, virtual `\DATAS\BGA\SPR.RES`
5. `open` failure `0x0002`: guest `piu.bin`, virtual `\DATAS\BGA\PIU.BIN`
6. `getcwd` success: drive `0x00`, virtual `\DATAS\BGA`
7. `getdrive` success: drive `0x02`

따라서 현재 도달 경로 안에서는 `\datas\bga`로 이동한 뒤 root로 되돌아가는 `chdir` 요청은 관측되지 않는다. `getcwd`도 요청대로 `DATAS\BGA`를 반환하고, `getdrive`는 현재 단일 virtual drive 정책에 따라 `C:`에 해당하는 `0x02`를 반환한다.

정적 문자열 관측에서는 `PIU.EXE` 안에 `\datas`, `\datas\bga`, `\datas\texture`, `\datas\model`, `\datas\step` 같은 절대 경로 문자열이 존재한다. 하지만 현재 실행 지점까지 실제 DOS path trace로 관측된 디렉터리 변경은 `\datas\bga` 1회뿐이다.

현재 로드 실패는 디렉터리 관련 요청 누락보다는, 원본 실행 흐름이 `\DATAS\BGA` current directory 상태에서 root에 존재하는 `SPR.RES`, `PIU.BIN`, `INTRO.ANI`, `stage.cfg`를 상대 경로로 열고 있다는 점에서 발생한다. 다음 결정은 root asset fallback 정책을 둘지, 또는 원본 cwd/파일 탐색 정책을 더 역추적할지이다.

## 검증

다음 명령으로 검증했다.

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

결과는 통과했다. Win32 x86 빌드 중 third-party `spdlog` header의 기존 코드 페이지 경고 `C4819`가 계속 표시되지만 이번 변경과 직접 관련된 실패는 아니다.

# DOS Path Request Trace Work Log

## Result

Added a recent DOS path request trace to inspect the directory/file request order made by `piu_1st`.

* Added `Win32DosPathTraceEntry` and `Win32DosPathObservation`.
* Added DOS path trace state to `Win32MinimalExecutionAttempt` and `ThreadContext`.
* Recorded trace entries from `chdir`, `getcwd`, `getdrive`, and `open`.
* Printed the trace stored count, limit state, and each trace entry in the Win32 loader log.
* Updated `scripts/test_all.ps1` to verify the current `piu_1st` path request order.

## Observation

The current `piu_1st` run observes this DOS path request order:

1. `chdir` success: guest `\datas\bga`, virtual `\DATAS\BGA`
2. `open` failure `0x0002`: guest `intro.ani`, virtual `\DATAS\BGA\INTRO.ANI`
3. `open` failure `0x0002`: guest `stage.cfg`, virtual `\DATAS\BGA\STAGE.CFG`
4. `open` failure `0x0002`: guest `spr.res`, virtual `\DATAS\BGA\SPR.RES`
5. `open` failure `0x0002`: guest `piu.bin`, virtual `\DATAS\BGA\PIU.BIN`
6. `getcwd` success: drive `0x00`, virtual `\DATAS\BGA`
7. `getdrive` success: drive `0x02`

So, within the currently reached path, there is no observed `chdir` request that returns from `\datas\bga` to root. `getcwd` returns `DATAS\BGA` as requested, and `getdrive` returns `0x02`, matching the current single-virtual-drive policy that exposes the root as `C:`.

Static string observation shows absolute path strings such as `\datas`, `\datas\bga`, `\datas\texture`, `\datas\model`, and `\datas\step` inside `PIU.EXE`. However, up to the current execution point, the only observed directory change is the single `\datas\bga` request.

The current load failure is therefore less likely to be a missed directory request. It is currently caused by the original execution flow opening root assets such as `SPR.RES`, `PIU.BIN`, `INTRO.ANI`, and `stage.cfg` as relative paths while the current directory is `\DATAS\BGA`. The next decision is whether to add a root asset fallback policy or to reverse-trace the original cwd/file-search policy further.

## Verification

Verified with:

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

The verification passed. The Win32 x86 build still reports the existing third-party `spdlog` code-page warning `C4819`, but it is not a failure caused by this change.
