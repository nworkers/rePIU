# Task 740: PIU10 MP3 재생 노이즈·노트 끊김 작업 지시

설계: [20260926-740](../design/20260926-740-piu10-mp3-playback-noise-and-note-stutter.md)

## 한국어

1. `Piu10Mp3AudioOut`에 토글 수, 다중 토글 횟수, PCM 큐 저수위·빈 큐 횟수 카운터를 넣고 통계와
   스냅샷에 노출한다.
2. `REPIU_PIU10_MP3_CENSUS_MS`로 live telemetry가 MP3 파이프라인 census 줄을 찍게 한다.
3. 최종 보고에 MP3 통계 줄을 낸다.
4. Linux x64 Release로 pumpitea attract 데모(자동으로 곡을 재생)를 40초 측정해 후보 A~D를 가른다.
5. 가려진 원인을 고치고 같은 측정으로 확인한다.
6. 설계·작업 로그·analysis를 갱신하고 커밋한다.

## English

1. Add toggle count, multi-toggle event, PCM low-water and empty-queue counters to `Piu10Mp3AudioOut`
   and expose them in its stats and snapshot.
2. Have the live telemetry print an MP3 pipeline census line under `REPIU_PIU10_MP3_CENSUS_MS`.
3. Print an MP3 statistics line in the final report.
4. Measure pumpitea's attract demo (it plays a song by itself) for 40 s on Linux x64 Release and
   separate candidates A-D.
5. Fix the cause the measurement names and confirm with the same measurement.
6. Update the design, work log and analysis, then commit.
