# piu_1st stage.cfg 누락 경로 resize guard 작업 로그

## 결과

`stage.cfg` open 실패를 정상 probe로 유지하면서, 그 직후 관측된 `INT 21h AH=0x4A`, `ES=0x0024`, `BX=0x4AE1` resize 요청을 insufficient memory로 실패시키는 guard를 추가했다.

처음에는 `BX >= 0x4AE1`을 일반 guard로 추가했지만, 너무 이른 resize 요청까지 실패해 `stage.cfg` 관측 전의 종료 경로로 빠지는 것을 확인했다. 따라서 guard를 마지막 DOS open이 실패한 `stage.cfg`일 때만 적용하도록 좁혔다.

traced DOS HLE에 `INT 21h AH=0x4C` process terminate 처리도 추가했다. 이 처리는 이른 종료 경로와 향후 정상 DOS 종료 경로를 host trampoline 복귀로 연결한다.

현재 `piu_1st`는 `stage.cfg` 실패 이후 `FAIL: res_load( spr.res )` 출력까지 진행한다. 마지막 open은 `spr.res`이며, 현재 current directory가 `\DATAS\BGA`이므로 `\DATAS\BGA\SPR.RES`를 찾다가 DOS error `0x0002`로 실패한다. 다음 중단 지점은 다시 `0x020F7340`의 `C7 01 FF FF FF FF` write이다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`: 일반 실행은 CMake stamp 파일 권한 문제로 실패했고, 권한 상승 실행은 성공했다.
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`: `FAIL: res_load( spr.res )`, last open `spr.res`, last resize `BX=0x70E1`, 현재 blocker `0x020F7340` 확인.
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`: 일반 실행은 CMake stamp 파일 권한 문제로 실패했고, 권한 상승 실행은 성공했다.

# piu_1st stage.cfg Missing-Path Resize Guard Work Log

## Result

Added a guard that preserves the failed `stage.cfg` open as a legitimate probe, then fails the observed `INT 21h AH=0x4A`, `ES=0x0024`, `BX=0x4AE1` resize request with insufficient memory.

The first attempt added a general `BX >= 0x4AE1` guard, but that also failed an earlier resize request and sent execution into a termination path before the `stage.cfg` observation. The guard was narrowed so it applies only when the last DOS open was the failed `stage.cfg` probe.

Also added traced DOS HLE handling for `INT 21h AH=0x4C` process termination. This connects early and future normal DOS termination paths back to the host trampoline.

`piu_1st` now advances past the `stage.cfg` failure to the `FAIL: res_load( spr.res )` output. The last open is `spr.res`; because the current directory is `\DATAS\BGA`, it attempts `\DATAS\BGA\SPR.RES` and fails with DOS error `0x0002`. The next stop is again the `C7 01 FF FF FF FF` write at `0x020F7340`.

## Verification

* `cmd /c scripts\build_win32_x86.bat`: failed in the normal run due to the CMake stamp-file permission issue, then passed with elevated permissions.
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`: confirmed `FAIL: res_load( spr.res )`, last open `spr.res`, last resize `BX=0x70E1`, and current blocker `0x020F7340`.
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`: failed in the normal run due to the CMake stamp-file permission issue, then passed with elevated permissions.
