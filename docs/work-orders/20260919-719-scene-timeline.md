# Task 719 작업 지시 — 장면별 시각을 두 host에서 비교

설계: [20260919-719](../design/20260919-719-scene-timeline.md)

## 범위

시간 기준 픽셀 표본(`REPIU_GLIDE_PIXEL_DIAG_INTERVAL_MS`)을 추가하고, 두 host의 장면별
지속 시간을 비교한다.

## 단계

1. Glide 백엔드에 시간 기준 표본과 4×4 grid 서명
2. 두 host 빌드·core probe
3. 두 host 180초 기록
4. 장면 경계 추출과 비교, 작업 로그, frontier

## 검증

* 변수가 없을 때 출력 변화 없음
* 두 host core probe

---

## English

Design: [20260919-719](../design/20260919-719-scene-timeline.md)

### Scope

Add time-based pixel sampling (`REPIU_GLIDE_PIXEL_DIAG_INTERVAL_MS`) and compare
per-scene durations on both hosts.

### Steps

1. Time-based sampling and a 4×4 grid signature in the Glide backend.
2. Builds and core probes on both hosts.
3. 180-second recordings on both hosts.
4. Extract and compare scene boundaries; work log and frontier.

### Verification

* No change in output when unset.
* Core probes on both hosts.
