# Task 746: 인자 실행의 `cfg/repiu.ini` 적용 작업 지시

설계: [20260927-746](../design/20260927-746-apply-ini-settings-on-argument-runs.md)

## 한국어

1. 로더 `main`의 비런처 경로에서 `cfg/repiu.ini`를 읽어 `PublishLauncherSettings`로 싣는다(환경 변수
   우선 유지, 경고 로그).
2. ini `swap_interval = 1`로 `repiu pumpitea`를 돌려 페이싱이 켜지는지, 환경 변수 `0`을 주면 이기는지,
   ini 없이는 그대로인지 확인한다. Win32 빌드·core probe를 확인한다.
3. README·설계·작업 로그를 갱신하고 커밋한다.

## English

1. On the loader's non-launcher path, read `cfg/repiu.ini` and publish it through
   `PublishLauncherSettings` (environment still wins; warnings logged).
2. With `swap_interval = 1` in the ini, check that `repiu pumpitea` enables pacing, that an
   environment value of `0` wins, and that nothing changes without an ini. Check the Win32 build and
   core probes.
3. Update the README, design and work log, then commit.
