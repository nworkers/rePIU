# Task 423 설계 — MSCDEX Stop을 멱등하게

**한 줄:** 이미 정지된 드라이브에 오는 `85h`(Stop)가 **직전에 세운 pause 상태를 스스로
지우고** 있었습니다. Task 422의 명령 trace가 초당 약 60회의 Stop을 잡아내면서 드러났습니다.

## 1. 근거

`CdAudioWaveOut::Stop()`이 매 호출마다 `paused = was_playing`으로 **재계산**했습니다.
따라서 첫 Stop은 `paused=1`(재생 중이었으므로), 두 번째부터는 `paused=0`입니다.
Task 421의 위치 census에 그대로 찍혔습니다 — `paused`가 `1, 0, 0, 0…`이고 `generation`은
100 ms당 6~7씩 올라 62까지 갔습니다.

MSCDEX 사양에서 `85h`는 **재생 중이면 멈추고 위치를 기억**하며, `88h`(Resume)가 그
지점에서 잇습니다. 이미 멈춘 드라이브에 대한 `85h`는 **아무 일도 하지 않아야** 합니다.

## 2. 변경

`playing`이 거짓이고 `paused`가 참이면 **즉시 반환**합니다. 플래그와 기억된 위치가
보존되고, `generation` 증가와 스트림 비우기도 일어나지 않습니다.

한 번도 재생한 적 없는 드라이브(`playing=0`·`paused=0`)에 오는 Stop은 기존 경로를
그대로 지나가므로 동작이 바뀌지 않습니다.

## 3. 판정

| 관측 | 통과 |
|---|---|
| census의 `paused` | 폭주 구간에서 **1로 유지** |
| census의 `generation` | 폭주 구간에서 **증가 없음** |
| 정상 프리뷰(`stop → ioctl 11 → play`) | 그대로 동작 |

---

# Task 423 Design — make MSCDEX Stop idempotent

**One line:** `85h` on an already-stopped drive **cleared the pause it had just established**,
found when Task 422's trace recorded about sixty Stops a second.

`CdAudioWaveOut::Stop()` recomputed `paused = was_playing` on every call, so the first Stop set
it and every later one cleared it — visible in Task 421's census as `paused` reading `1, 0, 0,
…` while `generation` climbed six or seven per 100 ms to 62. MSCDEX defines `85h` as stopping
playback and remembering the position for `88h` to resume from; on a drive that is already
stopped it must do nothing.

The fix returns immediately when `playing` is false and `paused` is true, preserving both the
flag and the remembered position and skipping the generation bump and stream clear. A Stop on a
drive that never played still takes the original path, so that behaviour is unchanged. It
passes when the census shows `paused` held at 1 with no generation movement during the storm,
and the normal preview sequence still works.
