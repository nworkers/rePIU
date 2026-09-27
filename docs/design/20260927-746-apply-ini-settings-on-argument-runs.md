# Task 746: 인자로 실행할 때도 `cfg/repiu.ini`를 먼저 읽어 적용한다

## 한국어

### 배경

`cfg/repiu.ini`의 `[Video] swap_interval`과 `[Audio]` 게인은 런처(인자 없이 실행)만 읽어서 게임
프로세스에 환경 변수(`REPIU_GLIDE_SWAP_INTERVAL`, `REPIU_YMZ_VOLUME`)로 넘긴다. `repiu pumpitea`처럼
인자를 주면 런처를 건너뛰므로 ini는 무시되고, 사용자는 같은 설정을 환경 변수로 다시 줘야 한다(Task
745에서 안내한 그대로). 사용자 요청: 인자 실행에서도 ini를 먼저 읽어 적용할 것.

### 설계

1. 로더 `main`에서 런처 경로가 아닐 때(인자가 있거나 `REPIU_LAUNCHER=0`) target을 고르기 전에
   `LoadLauncherSettings("cfg")`를 읽고, 기존 `PublishLauncherSettings`(`ApplyLauncherSettings` →
   `PublishEnvironmentSetting`)로 같은 환경 변수에 싣는다. 파일이 없으면 아무것도 하지 않는다.
   경고(형식 오류)는 로그로 낸다.
2. **환경 변수가 이긴다**는 규칙은 그대로다. `ResolveLauncherEnvironmentOverrides`가 호출자 환경에 이미
   있는 변수를 보고 그 항목은 건드리지 않는다. 런처가 띄운 자식 프로세스도 이 경로를 지나지만 부모가
   실린 변수를 호출자 값으로 보므로 결과는 같다.
3. 로그에 `Launcher settings published swap-interval/volume: …`와 ini 경로를 남겨, 어느 값이 어디서
   왔는지 보이게 한다.

### 검증 전략

`cfg/repiu.ini`에 `swap_interval = 1`을 두고 `repiu pumpitea`(환경 변수 없음)를 돌려 페이싱이 켜지는지,
`REPIU_GLIDE_SWAP_INTERVAL=0`을 함께 주면 환경 변수가 이겨 페이싱이 없는지, ini가 없을 때 이전과
같은지, Win32 빌드·core probe가 통과하는지 본다.

## English

### Background

`[Video] swap_interval` and the `[Audio]` gain in `cfg/repiu.ini` are read only by the launcher (a run
without arguments), which hands them to the game process as environment variables
(`REPIU_GLIDE_SWAP_INTERVAL`, `REPIU_YMZ_VOLUME`). A run with arguments such as `repiu pumpitea` skips
the launcher, so the ini is ignored and the user has to repeat the setting as an environment variable
(as Task 745 advised). The user asked for argument runs to read and apply the ini first.

### Design

1. In the loader's `main`, on the non-launcher path (arguments given, or `REPIU_LAUNCHER=0`), read
   `LoadLauncherSettings("cfg")` before the target is chosen and publish it through the existing
   `PublishLauncherSettings` (`ApplyLauncherSettings` → `PublishEnvironmentSetting`). A missing file
   does nothing; format warnings are logged.
2. **The environment still wins**: `ResolveLauncherEnvironmentOverrides` sees variables already in
   the caller's environment and leaves those items alone. The launcher's child process also passes
   through this path, but sees the parent's published variables as caller-set, so the outcome is
   the same.
3. The log names the ini path and prints `Launcher settings published swap-interval/volume: …`, so
   it is visible where a value came from.

### Verification strategy

With `swap_interval = 1` in `cfg/repiu.ini`, `repiu pumpitea` with no environment variable enables
pacing; with `REPIU_GLIDE_SWAP_INTERVAL=0` as well, the environment wins and there is no pacing;
without an ini nothing changes; Win32 build and core probes pass.

---

## 결과 (구현 후) / Result (after implementation)

### 한국어

설계대로 넣었습니다. ini만 있으면 `repiu pumpitea`가 `published swap-interval/volume: true/false`를
찍고 페이싱이 켜져 60 fps로 잡히며, `REPIU_GLIDE_SWAP_INTERVAL=0`을 함께 주면 `environment wins:
swap-interval env`로 환경 변수가 이깁니다. 설계와 달라진 점은 없습니다.

### English

Implemented as designed. With only the ini, `repiu pumpitea` prints `published swap-interval/volume:
true/false`, pacing is on and the frame rate holds at 60; with `REPIU_GLIDE_SWAP_INTERVAL=0` as well,
`environment wins: swap-interval env` shows the environment taking precedence. No departures from the
design.
