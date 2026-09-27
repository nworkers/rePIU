# Task 746 작업 로그 — 인자로 실행할 때도 `cfg/repiu.ini`를 먼저 읽어 적용

설계: [20260927-746](../design/20260927-746-apply-ini-settings-on-argument-runs.md)
작업 지시: [20260927-746](../work-orders/20260927-746-apply-ini-settings-on-argument-runs.md)

## 요약

`repiu pumpitea`처럼 인자를 주고 실행해도 `cfg/repiu.ini`의 `swap_interval`과 게인이 적용됩니다.
로더 `main`의 비런처 경로가 target을 고르기 전에 `LoadLauncherSettings("cfg")`를 읽고, 런처가 쓰던
`PublishLauncherSettings`로 같은 환경 변수에 싣습니다. 호출자 환경에 이미 있는 변수는 파일보다
우선합니다(`ResolveLauncherEnvironmentOverrides`). 로그의 `Launcher settings read from cfg/repiu.ini
for an argument run (environment wins: swap-interval/volume file/env)`가 어느 값이 어디서 왔는지
말합니다.

## 과정

1. 런처 경로만 ini를 읽고 있었습니다(`LauncherRequested(argc)`가 참일 때). 인자 실행은 그 블록을
   지나쳐 `SelectTargetProfile`로 갔습니다.
2. 그 사이에 ini 읽기·경고 로그·publish를 넣었습니다. 파일이 없으면(`file_present=false`) 아무것도
   하지 않습니다. 런처가 띄운 자식은 부모가 실은 변수를 호출자 값으로 보므로 결과가 같습니다.
3. README의 vsync 문단을 Task 745의 "환경 변수를 직접 주라"에서 "ini도 인자 실행에 적용된다"로
   바꿨습니다.

## 검증

| 검증 | 결과 |
|---|---|
| ini `swap_interval = 1`, 환경 변수 없음, `repiu pumpitea` | `published swap-interval/volume: true/false`, override requested=1, 페이싱 활성(59.98 Hz, 16,672 µs), fps 60.0 |
| 같은 ini + `REPIU_GLIDE_SWAP_INTERVAL=0` | `environment wins: swap-interval env`, published false, override requested=0, 페이싱 없음, fps 107~127 |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug 빌드 + core probe | 아래 English 절과 같음 |

ini가 없는 경우는 사용자의 파일을 옮기지 않으려 따로 돌리지 않았습니다. 코드는 `file_present`가
거짓이면 경로를 건너뜁니다.

로그: `build/task746-ini-noenv.err.log`, `build/task746-ini-env0.err.log`.

---

# English

# Task 746 work log — applying `cfg/repiu.ini` on argument runs too

Design: [20260927-746](../design/20260927-746-apply-ini-settings-on-argument-runs.md)
Work order: [20260927-746](../work-orders/20260927-746-apply-ini-settings-on-argument-runs.md)

## Summary

A run with arguments such as `repiu pumpitea` now applies the `swap_interval` and gain in
`cfg/repiu.ini`. The loader's non-launcher path reads `LoadLauncherSettings("cfg")` before choosing
the target and publishes it through the launcher's `PublishLauncherSettings` into the same
environment variables. A variable already in the caller's environment wins over the file
(`ResolveLauncherEnvironmentOverrides`). The log's `Launcher settings read from cfg/repiu.ini for an
argument run (environment wins: swap-interval/volume file/env)` says where each value came from.

## Steps

1. Only the launcher path read the ini (when `LauncherRequested(argc)` held); argument runs went past
   that block straight to `SelectTargetProfile`.
2. The read, warning log and publish went in between. A missing file (`file_present=false`) does
   nothing. The launcher's child sees the parent's published variables as caller-set, so its outcome
   is unchanged.
3. The README's vsync paragraph changed from Task 745's "set the variable yourself" to "the ini
   applies to argument runs too".

## Verification

| Check | Result |
|---|---|
| ini `swap_interval = 1`, no environment, `repiu pumpitea` | `published swap-interval/volume: true/false`, override requested=1, pacing active (59.98 Hz, 16,672 µs), 60.0 fps |
| same ini + `REPIU_GLIDE_SWAP_INTERVAL=0` | `environment wins: swap-interval env`, published false, override requested=0, no pacing, 107–127 fps |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug build + core probe | build succeeded, `core_probe_all=true` |

The no-ini case was not run separately, to leave the user's file in place; the code skips the path
when `file_present` is false.

Logs: `build/task746-ini-noenv.err.log`, `build/task746-ini-env0.err.log`.
