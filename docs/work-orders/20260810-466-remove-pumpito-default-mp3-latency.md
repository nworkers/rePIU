# 20260810-466 pumpito MP3 기본 지연 제거 작업 지시 / Remove pumpito Default MP3 Latency Work Order

설계: [20260810-466-remove-pumpito-default-mp3-latency.md](../design/20260810-466-remove-pumpito-default-mp3-latency.md)

## 한국어

- [x] `pumpito` profile의 명시적 50 ms 기본값을 제거합니다.
- [x] profile probe와 현재 아키텍처·누적 분석을 0 ms 기본값으로 갱신합니다.
- [x] PIU10 probe와 Win32 x86 Debug 빌드를 검증합니다.
- [x] 변경을 `main`에 커밋하고 `v0.0.146` tag를 새 커밋으로 이동합니다.

## English

- [x] Remove the explicit 50 ms default from the `pumpito` profile.
- [x] Update the profile probe, current architecture, and cumulative analysis for the 0 ms default.
- [x] Verify the PIU10 probe and Win32 x86 Debug build.
- [x] Commit directly to `main` and move tag `v0.0.146` to the new commit.
