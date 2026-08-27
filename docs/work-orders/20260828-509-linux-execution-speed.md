# Task 509 작업 지시 — Linux 실행 속도

설계: [20260828-509](../design/20260828-509-linux-execution-speed.md) ·
작업 로그: [20260828-509](../work-logs/20260828-509-linux-execution-speed.md)

## 0. 시작 전에 — 잴 것이 없다는 것부터 확인하십시오

```bash
grep -aiE "frame|swap" build/task508-after-run6.err | tail -4
```

프레임 수가 **없어야** 합니다. 있으면 누가 이미 고친 것이고 이 지시서가 낡은 것입니다.

## 1. 표시된 프레임의 총계와 시작 시각을 남기십시오

`glide_opengl_backend.{h,cpp}`의 `RecordPresentedFrame()`입니다. 지금 있는
`frame_rate_frame_count_`는 **초마다 1로 리셋되므로 총계가 아닙니다.** 별도로 둡니다.

* `presented_frame_total_` — 단조 증가.
* `presented_frame_first_` — 첫 프레임의 `steady_clock` 시각.

접근자 둘을 공개하십시오. **호스트 스레드 전용이라는 계약을 주석에 적으십시오** — 그것이
종료 블록에서 락 없이 읽어도 되는 이유입니다.

## 2. 종료 줄에 실으십시오

`execution_trampoline.cpp`의 `[repiu-shutdown]` 줄에 `frames=`와 `span_ms=`를 더합니다.
**줄이 갈라지기 전, 두 갈래 모두가 지나는 자리**입니다. 버퍼 크기(256)를 넘기지 않는지
확인하십시오.

`span_ms`는 **첫 프레임부터**입니다. 예산 전체로 나누지 마십시오 — `pumpit1`은 기동에 약
45초를 씁니다.

## 3. 초당 FPS 로그를 선택적으로 내십시오

`REPIU_GLIDE_FRAME_RATE_LOG=1`일 때만, 창 제목에 쓰는 것과 **같은 값**을 오류 스트림에
한 줄 냅니다. 꺼져 있으면 문자열을 만들지도 마십시오.

env 조회를 프레임마다 하지 마십시오. 한 번 읽어 두는 자리는 backend 초기화입니다.

## 4. 재십시오 — 그리고 조건을 어기지 마십시오

| 항목 | 값 |
|---|---|
| 빌드 | Release, 양쪽 호스트 (안 되면 Debug 대 Debug, **명시할 것**) |
| vsync | `REPIU_GLIDE_SWAP_INTERVAL=0` |
| 감시견 | `REPIU_STALL_TIMEOUT_MS=0` |
| 장면·예산 | `pumpit1`, 같은 값 |
| 반복 | 호스트당 3회 |

**Linux Release 빌드는 해 본 적이 없습니다.** 여기서 막히면 그것 자체가 소견이므로 작업
로그에 남기고 Debug로 내려가십시오.

보고할 것은 **배율 하나**입니다 — "Linux가 Windows의 몇 분의 일인가". 범위가 겹치면
겹친다고 적으십시오.

## 5. 하지 마십시오

* **귀속 노브를 켜지 마십시오.** 배율을 모르는 채 켠 프로파일은 읽을 기준이 없습니다. 510의
  일입니다.
* **Windows의 순위를 Linux 결론으로 옮기지 마십시오.**
* 한 번 실행을 판정으로 쓰지 마십시오.

## 6. 문서

* frontier 4절의 "그다음은 속도입니다"에 측정값을 넣고, 다음 항목(귀속)을 남기십시오.
* 반복 절차는 `docs/guides/`에 둡니다 — 다음 세션이 같은 조건을 다시 세우지 않도록.

---

# Task 509 work order — Linux execution speed

Design: [20260828-509](../design/20260828-509-linux-execution-speed.md) ·
Work log: [20260828-509](../work-logs/20260828-509-linux-execution-speed.md)

## 0. Before starting — confirm there is nothing to measure

```bash
grep -aiE "frame|swap" build/task508-after-run6.err | tail -4
```

There should be **no** frame count. If there is, someone has already fixed it and this order is out
of date.

## 1. Keep the presented-frame total and its start time

In `RecordPresentedFrame()` in `glide_opengl_backend.{h,cpp}`. The existing
`frame_rate_frame_count_` **resets to 1 every second and is not a total**, so keep separate fields:

* `presented_frame_total_` -- monotonic.
* `presented_frame_first_` -- the `steady_clock` time of the first frame.

Expose two accessors, and **write the host-thread-only contract in the comment** -- that is the
reason the shutdown block may read them without a lock.

## 2. Put them on the shutdown line

Add `frames=` and `span_ms=` to the `[repiu-shutdown]` line in `execution_trampoline.cpp`, **before
the arms split, where both paths pass**. Check the 256-byte buffer still fits.

`span_ms` runs **from the first frame**. Do not divide by the whole budget -- `pumpit1` spends about
45 seconds starting up.

## 3. Make the per-second FPS loggable

Only when `REPIU_GLIDE_FRAME_RATE_LOG=1`, write the **same value** that goes to the window title as
one line on the error stream. With it off, do not even build the string.

Do not read the environment per frame; read it once, where the backend initialises.

## 4. Measure -- and do not bend the conditions

| Item | Value |
|---|---|
| Build | Release on both hosts (if impossible, Debug against Debug, **stated explicitly**) |
| vsync | `REPIU_GLIDE_SWAP_INTERVAL=0` |
| Watchdog | `REPIU_STALL_TIMEOUT_MS=0` |
| Scene and budget | `pumpit1`, the same values |
| Repeats | three per host |

**A Linux Release build has never been attempted.** If it blocks here, that is itself a finding:
record it in the work log and fall back to Debug.

What gets reported is **one factor** -- what fraction of Windows Linux runs at. If the ranges
overlap, say so.

## 5. Do not

* **Do not turn the attribution knobs on.** A profile taken without knowing the factor has nothing to
  be read against. That is 510's work.
* **Do not carry Windows' ranking over as a Linux conclusion.**
* Do not treat a single run as a verdict.

## 6. Documentation

* Put the measurement into the frontier's "After this, speed" section and leave the next item
  (attribution) named.
* Put the repeatable procedure under `docs/guides/` so the next session does not rebuild the same
  conditions from scratch.
