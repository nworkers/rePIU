# Timeout DOS observation copy 작업 지시

## 목표

`piu_1st` timeout 결과에서도 정상 종료 경로와 동일한 DOS/HLE 관측 필드가 출력되도록 한다.

## 범위

* `ThreadContext`에서 `Win32MinimalExecutionAttempt`로 관측 상태를 복사하는 공용 helper를 만든다.
* timeout 경로와 정상 종료 경로가 같은 helper를 사용하게 한다.
* chdir/open/ioctl/resize 세부 정보가 timeout 결과에도 반영되도록 한다.
* `piu_1st` 실행 결과에서 chdir 도달 여부와 현재 마지막 DOS service를 확인한다.

## 제외 범위

* chdir 지점까지 강제로 진행시키기 위한 새 opcode/HLE 구현은 포함하지 않는다.
* timeout 정책 자체를 장기 실행 모델로 바꾸지 않는다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Timeout DOS Observation Copy Work Order

## Goal

Make timeout results print the same DOS/HLE observation fields as the normal completion path for `piu_1st`.

## Scope

* Add a shared helper that copies observation state from `ThreadContext` into `Win32MinimalExecutionAttempt`.
* Use the same helper from both timeout and normal completion paths.
* Ensure chdir/open/ioctl/resize details are reflected in timeout results.
* Check whether the current `piu_1st` run reaches chdir and what the last DOS service is.

## Out Of Scope

* Do not add new opcode/HLE behavior just to force progress to the chdir point.
* Do not turn the timeout policy into the long-term execution model.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
