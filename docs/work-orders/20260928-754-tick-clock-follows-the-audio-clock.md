# Task 754: tick 시계를 오디오의 시계로 작업 지시

설계: [20260928-754](../design/20260928-754-tick-clock-follows-the-audio-clock.md)

## 한국어

1. `include/repiu/engine/event_clock.h`에 스위치(`ResolveEventClockUsesSteady`), timestamp 옮기기
   (`TranslateEventTimestamp`), 어긋남 계산과 창 단위 계측(`HostClockDivergenceMeter`)을 둔다.
2. `GlideOpenGlBackend::EventClockNanoseconds`가 `steady_clock`을 돌려주고(`REPIU_EVENT_CLOCK=sdl`로 이전
   동작), 키 이벤트의 timestamp를 `EventTimestampNanoseconds`로 옮겨 타임라인에 기록한다.
3. 이벤트 펌프에서 두 시계를 비교해 stderr 경고(한 번)와 최종 보고 줄을 남긴다.
4. probe `event_clock`을 core probe와 aot probe에 더한다.
5. Linux x64·Win32에서 빌드와 probe를 확인하고, pumpit8 입력 스크립트 1회로 tick 속도를 본 뒤
   README·analysis·작업 로그를 갱신하고 커밋한다.

## English

1. `include/repiu/engine/event_clock.h` holds the switch (`ResolveEventClockUsesSteady`), the translation
   of timestamps (`TranslateEventTimestamp`), and the divergence arithmetic and metering by window
   (`HostClockDivergenceMeter`).
2. `GlideOpenGlBackend::EventClockNanoseconds` returns `steady_clock` (`REPIU_EVENT_CLOCK=sdl` for the old
   behaviour), and key events' timestamps are translated by `EventTimestampNanoseconds` before the
   timeline records them.
3. The event pump compares the two clocks and leaves a warning on stderr (once) and a line in the final
   report.
4. The probe `event_clock` joins the core probe and the aot probe.
5. Check the builds and the probes on Linux x64 and Win32, read the tick rate from one scripted pumpit8
   run, update README, the analysis and the work log, and commit.
