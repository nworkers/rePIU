# piu_1st stage.cfg 누락 경로 resize guard 작업 지시

## 목표

`stage.cfg` 누락을 정상 probe로 유지하면서, 이후 `ES=0x0024`, `BX=0x4AE1` resize 성공으로 인해 arena 밖 memory write가 발생하는 흐름을 막고 다음 관측 지점까지 진행한다.

## 범위

* traced DOS resize HLE에서 `stage.cfg` 실패 이후에만 적용되는 관측 기반 `BX >= 0x4AE1` 실패 guard를 추가한다.
* 실패 결과는 DOS insufficient memory `AX=0x0008`, carry flag set, 최대 가능 paragraph `BX=0x4AE0`으로 응답한다.
* `stage.cfg` open 실패는 그대로 유지한다.
* traced DOS HLE에 `INT 21h AH=0x4C` process terminate 최소 처리를 추가한다.
* `scripts/test_all.ps1`의 `piu_1st` 기대 관측점을 갱신한다.
* `docs/TODO.md`와 작업 로그를 현재 상태로 갱신한다.

## 검증

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`로 새 관측 지점을 확인한다.
* `scripts/test_all.ps1`로 `dos4gw_hello`와 `piu_1st` 현재 기대 상태를 확인한다.

# piu_1st stage.cfg Missing-Path Resize Guard Work Order

## Goal

Preserve the missing `stage.cfg` probe while preventing the out-of-arena memory write caused by treating the subsequent `ES=0x0024`, `BX=0x4AE1` resize request as successful.

## Scope

* Add an observed `BX >= 0x4AE1` failure guard to traced DOS resize HLE, scoped to the path after the failed `stage.cfg` probe.
* Return DOS insufficient memory with `AX=0x0008`, carry flag set, and maximum available paragraphs in `BX=0x4AE0`.
* Keep the `stage.cfg` open failure unchanged.
* Add minimal traced DOS HLE handling for `INT 21h AH=0x4C` process termination.
* Update the `piu_1st` expected observation point in `scripts/test_all.ps1`.
* Update `docs/TODO.md` and the work log with the current state.

## Verification

* Run `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st` to confirm the new observation point.
* Run `scripts/test_all.ps1` to verify `dos4gw_hello` and the current expected `piu_1st` state.
