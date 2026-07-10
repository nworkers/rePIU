# 진단 진행 기반 polling 작업 지시

## 목표

`piu_1st`가 실제 environment block을 스캔하는 동안 고정 polling 반복 횟수 때문에 너무 빨리 timeout되지 않도록, 진행 관측 기반 polling을 추가한다.

## 범위

* Win32 실행 context에 atomic diagnostic progress counter를 추가한다.
* DOS environment access가 관측될 때 progress counter를 증가시킨다.
* polling loop가 progress counter와 single-step count 변화를 기준으로 조용한 반복 수를 계산한다.
* 진행이 계속 있으면 caller timeout millisecond까지 관측을 연장한다.
* loader와 테스트에 diagnostic polling 관측값을 추가한다.

## 정책

* guest thread가 실행 중인 동안 host polling loop는 atomic counter만 읽는다.
* environment 값 원문은 계속 출력하지 않는다.
* timeout 후 상세 attempt 복사와 guest thread 정리는 기존 흐름을 유지한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Diagnostic Progress Polling Work Order

## Goal

Add progress-aware polling so `piu_1st` does not time out too early from a fixed polling iteration count while it is scanning a real environment block.

## Scope

* Add an atomic diagnostic progress counter to the Win32 execution context.
* Increment the progress counter when DOS environment accesses are observed.
* Make the polling loop compute quiet iterations from progress counter and single-step count changes.
* Extend observation up to the caller timeout in milliseconds while progress continues.
* Add diagnostic polling observations to loader output and tests.

## Policy

* While the guest thread is running, the host polling loop reads only atomic counters.
* Continue not printing raw environment values.
* Keep the existing timeout observation copy and guest thread cleanup flow.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
