# Task 423 작업 지시 — MSCDEX Stop 멱등화

설계: [20260805-423](../design/20260805-423-cd-audio-stop-idempotency.md)

## 1. 구현

`src/platform/win32/cd_audio_wave_out.cpp`의 `Stop()` 한 곳. `playing`이 거짓이고
`paused`가 참이면 조기 반환합니다. 그 외 경로는 변경하지 않습니다.

## 2. 검증 (완료)

Task 421 census + Task 422 trace를 켜고 pumpit2 3회.

| 지표 | 수정 전 | 수정 후 |
|---|---|---|
| `paused` | `1 → 0, 0, 0…` | **1 유지** |
| `generation` | 62까지 상승 | **5에 고정** |
| 프리뷰 동작 | 정상 | 정상 |

## 3. 완료 기준

1. 위 표가 재현됩니다. — **완료**
2. 작업 로그에 결과를 남깁니다. — [20260805-421-423](../work-logs/20260805-421-423-cd-audio-and-stall-root-cause.md)

---

# Task 423 Work Order — make MSCDEX Stop idempotent

One place in `Stop()`: return early when `playing` is false and `paused` is true, leaving every
other path alone. Verified across three pumpit2 runs with Task 421's census and Task 422's
trace enabled: `paused` now holds at 1 instead of reading `1, 0, 0, …`, `generation` stays at 5
instead of climbing to 62, and the normal preview sequence is unaffected.
