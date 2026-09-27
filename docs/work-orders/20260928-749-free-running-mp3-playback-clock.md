# Task 749: 자유 진행 MP3 재생 시계 작업 지시

설계: [20260928-749](../design/20260928-749-free-running-mp3-playback-clock.md)

## 한국어

1. `Piu10Mp3AudioOut`에 위치 트레이스(`REPIU_PIU10_MP3_POSITION_TRACE=1`)를 넣는다: 토글마다, 게스트가
   새 값을 본 읽기마다 한 줄.
2. `PlaybackPosition`을 자유 진행 + 4초 시정수 슬루(±2%) 시계로 바꾼다. 목표는 가져간 count − 장치 버퍼
   절반. 불변식(상한, 역행 금지)은 유지한다.
3. wayland·x11 자유 실행·x11 페이싱 플레이(곡 중 입력 스크립트)로 토글 간격·관측 지연을 재고, core
   probe(Linux·Win32)를 돌린 뒤 README·analysis·작업 로그를 갱신하고 커밋한다.

## English

1. Add the position trace (`REPIU_PIU10_MP3_POSITION_TRACE=1`) to `Piu10Mp3AudioOut`: one line per
   toggle and one per guest read that first sees the new value.
2. Make `PlaybackPosition` a free-running clock with a four second slew (±2%) toward the pulled count
   less half a device buffer, keeping the invariants (cap, no reverse).
3. Remeasure toggle intervals and observation delays in wayland, x11 free-running and x11 paced plays
   (the in-song input script), run the core probes (Linux, Win32), update README, the analysis and the
   work log, and commit.
