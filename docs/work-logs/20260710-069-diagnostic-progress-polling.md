# 진단 진행 기반 polling 작업 로그

## 변경 내용

Win32 guest 실행 polling을 고정 반복 횟수 중심에서 진행 관측 기반으로 보강했다.

`ThreadContext`에 atomic diagnostic progress counter를 추가하고, DOS environment block 접근이 관측될 때마다 counter를 증가시켰다. host polling loop는 guest thread가 실행 중일 때 상세 environment 필드를 직접 읽지 않고, atomic progress counter와 single-step count만 읽어 진행 여부를 판단한다.

polling loop는 진행이 없으면 조용한 반복 한도 `100000`회에서 timeout으로 판단하고, 진행이 계속 있으면 caller가 넘긴 timeout millisecond까지 관측을 이어간다. loader에는 poll iteration 수, progress count, quiet iteration 수를 출력하도록 했다.

`scripts/test_all.ps1`는 progress polling 관측값과 함께, 실제 environment scan 이후 `piu_1st`가 다시 `\datas\bga` chdir, `spr.res` open 실패, resize, memory store 지점까지 도달하는지 확인하도록 보강했다.

## 결과

`piu_1st`는 실제 DOS environment block을 읽은 뒤 다시 DOS 파일 경로 관측 지점까지 도달했다.

최근 수동 실행에서 관측된 대표 값은 다음과 같다.

* diagnostic poll iterations: 약 `328903`
* diagnostic progress count: 약 `7203`
* diagnostic quiet iterations: `100000`
* last DOS environment entry: `WINDIR=<redacted>`
* handled DOS chdir count: `1`
* last DOS chdir guest path: `\datas\bga`
* handled DOS open count: `3`
* last DOS open guest path: `spr.res`
* last DOS open result: failure, error `0x0002`

`spr.res` 파일이 없는 것은 현재 자산 상태에서 정상 관측으로 유지된다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 결과: 성공
  * 참고: 기존 spdlog code page 경고는 유지됨
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: 성공
  * 확인: progress polling 후 chdir/open 관측 재도달
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 결과: 성공

# Diagnostic Progress Polling Work Log

## Changes

Strengthened Win32 guest execution polling from a fixed-iteration-only policy to a progress-aware diagnostic policy.

Added an atomic diagnostic progress counter to `ThreadContext`, incremented whenever a DOS environment block access is observed. While the guest thread is running, the host polling loop reads only the atomic progress counter and the single-step count to decide whether execution is still making progress.

The polling loop treats the run as timed out when no progress is observed for `100000` quiet iterations. If progress continues, it keeps observing until the caller-provided timeout in milliseconds is reached. The loader now prints poll iteration count, progress count, and quiet iteration count.

`scripts/test_all.ps1` now checks the progress polling observations and also verifies that, after the real environment scan, `piu_1st` again reaches the `\datas\bga` chdir, `spr.res` open failure, resize, and memory store observation points.

## Result

`piu_1st` now reaches the DOS file path observation point again after reading the real DOS environment block.

Representative values from a recent manual run:

* diagnostic poll iterations: about `328903`
* diagnostic progress count: about `7203`
* diagnostic quiet iterations: `100000`
* last DOS environment entry: `WINDIR=<redacted>`
* handled DOS chdir count: `1`
* last DOS chdir guest path: `\datas\bga`
* handled DOS open count: `3`
* last DOS open guest path: `spr.res`
* last DOS open result: failure, error `0x0002`

The missing `spr.res` file remains the expected observation for the current asset state.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Result: passed
  * Note: the existing spdlog code page warning remains
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: passed
  * Checked: progress polling reached chdir/open observations again
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Result: passed
