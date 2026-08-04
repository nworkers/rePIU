# Task 420 작업 로그 — 남은 Glide draw 진입점 (**구현 완료, 회귀 검증은 미완**)

설계: [20260805-420](../design/20260805-420-glide-remaining-draw-entry-points.md) ·
작업 지시: [20260805-420](../work-orders/20260805-420-glide-remaining-draw-entry-points.md)

## 1. 한 줄 결과

미구현 draw 진입점 **아홉 개 중 일곱 개**를 구현했고, pumpit3에서
`unimplemented` 카운터가 **1 → 0**이 됐습니다. 다만 검증 중 **이번 변경과 무관한 별개의
실행 정지**를 발견해, 작업 지시가 요구한 프레임 회귀 확인은 **완료하지 못했습니다**.

## 2. 로그 분석 — 사용자 제공 `repiu_log.txt`(pumpit2, UTF-16LE)

```
GLIDE_UNIMPLEMENTED_FUNCTION ordinal=71 name=_GRDRAWPOINT@4 reason=draw-point-noop
Win32 Glide implementation issues .../unique/overflow: 2/0/0/0/1/0
Win32 Glide call trace: ordinal=71 name=_GRDRAWPOINT@4 count=2
```

이 실행이 호출한 **41개 ordinal 중 미구현은 `grDrawPoint` 하나**(2회)뿐이었습니다.
코드를 확인하니 draw 계열 아홉 개 중 실제로 그리는 것은 둘뿐이었고, 나머지 일곱은
요청을 받아 삼키고 있었습니다.

## 3. 변경

| 파일 | 내용 |
|---|---|
| `glide_vertex.h` | `kMaxGlidePolygonVertices = 64` |
| `glide_opengl_backend.h/.cpp` | `DrawPoint`(1·`GL_POINTS`), `DrawPolygon`(n·`GL_TRIANGLE_FAN`) |
| `linexe_glide_boundary.cpp` | point(+AA), AA line, AA triangle, polygon 6종. draw milestone에 AA 구간 추가. A/B 스위치 |

새 렌더링 경로는 만들지 않았습니다 — `DrawPrimitive`가 이미 정점 수와 GL primitive에
대해 일반적이어서 backend 함수 둘만 추가하면 됐습니다.

**판단 두 가지를 남깁니다.** AA 변종은 **AA 플래그를 무시하고 같은 기하**를 그립니다
(근사이지만 무동작보다 명백히 정확). 폴리곤의 `GL_TRIANGLE_FAN`은 Glide가 볼록
다각형을 요구하므로 **근사가 아니라 정확**합니다.

**ABI 불변:** 각 case의 `Esp` 증가량은 서명표의 `argument_byte_count`와 일치합니다
(`@4`→2, `@8`→3, `@12`→4, `@24`→7 slot).

## 4. 검증 — 통과한 것

| 항목 | 결과 |
|---|---|
| `repiu_glide_render_probe` | **pass** |
| `repiu_glide_issue_probe` | **pass** |
| `unimplemented` 카운터 | A/B에서 off `1/0/0/0/1/0` → **on `0/0/0/0/0/0`** |
| 새 decline 사유 | **0건** |

## 5. 발견 — 별개의 실행 정지, 그리고 그것이 이번 변경이 아님

새 빌드로 돌린 pumpit3/pumpit2가 60초를 채우지 못하고 **13~33초에 멈췄습니다**.
종료 사유는 타임아웃이 아니라 `PollThreadUntilExit`의 **1초 무진행 감시**입니다.

**A/B가 인과를 배제했습니다** (`REPIU_GLIDE_DRAW_ENTRY_POINTS`, 같은 바이너리, 교대):

| 조건 | 프레임 | 정지 | issues |
|---|---:|---:|---|
| off(예전 동작) | 403 / 446 / 419 | 13.5 / 13.4 / 12.9초 | 1/0/0/0/1/0 |
| on(새로 그림) | 427 / 430 / 449 | 13.4 / 13.6 / 13.6초 | **0/0/0/0/0/0** |

**그리기를 꺼도 똑같이 멈춥니다.** 두 조건이 구분되지 않습니다.

**함께 배제한 것:**

| 후보 | 근거 | 판정 |
|---|---|---|
| Task 419 스핀 | `spin=0`도 정지(20.0 / 27.0초) | 배제 |
| EEPROM 상태 | fixture가 Task 418·419 사본과 **바이트 동일**(mtime만 갱신) | 배제 |
| 잔류 프로세스 | 실행 전 0개 확인 | 배제 |
| `grDrawPoint` 호출 수 | 2회 그린 실행이 **더 오래**(33초, 1,296프레임) 돌았음 | 배제(첫 가설 반증) |

**정지의 정체는 "덜 간 것"이 아니라 "더 간 것"입니다.** 60초를 완주하던 Task 419
실행은 DOS path trace가 **10개**(마지막 `bga\16.dat`)인데, 13초에 멈춘 실행은
**16~19개**로 `title\t301~t305.ptx` → `.\step\mix4_1.NOT` → `bga\30.dat`까지
갑니다. 이는 **attract 데모 gameplay 진입**이며, 이전 실행들은 60초 안에 도달하지
못하던 지점입니다.

**미확정 — 왜 지금 그 지점에 도달하는가.** 게스트 시간 진행 속도, 입력, 환경 중
무엇인지 이 작업에서는 가리지 못했습니다. frontier에 별도 항목으로 올립니다.

## 6. 완료하지 못한 것

작업 지시 §3의 **pumpit3 프레임 회귀 확인(약 3,036~3,084 유지)** 은 **수행하지
못했습니다.** 정지 때문에 60초 표본을 얻을 수 없기 때문입니다. 대신 같은 조건 안의
A/B로 **그리기 on/off가 구분되지 않음**을 보였고, 이것이 지금 줄 수 있는 최대치입니다.
정지가 해소되면 회귀 확인을 다시 해야 합니다.

## 7. 회고

* **A/B 스위치가 또 한 번 판정을 대신했습니다.** "새 코드가 범인"이라는 그럴듯한
  서사(로그가 지목한 API를 구현했더니 멈춤)를 한 번의 측정이 뒤집었습니다.
* **첫 상관관계를 믿지 않은 것이 맞았습니다.** `drawpoint=1`인 실행만 멈춘다고 봤는데,
  `drawpoint=2`인 실행이 더 오래 돌아 그 해석을 반증했습니다. 호출 수는 원인이 아니라
  **지속 시간의 결과**였습니다.
* **"멈췄다"를 "덜 갔다"로 읽지 않은 것이 핵심이었습니다.** DOS trace 수를 비교하지
  않았다면 정지 지점을 계속 부팅 근처에서 찾았을 것입니다.

---

# Task 420 Work Log — the remaining Glide draw entry points (**implemented; regression check incomplete**)

## 1. Result in one line

**Seven of the nine** draw entry points now draw, and pumpit3's `unimplemented` counter goes
**1 to 0** — but verification uncovered **a separate execution stall, unrelated to this
change**, which left the work order's frame-regression check **unfinished**.

## 2. Log analysis

The user's `repiu_log.txt` (pumpit2, UTF-16LE) reports `_GRDRAWPOINT@4` under
`GLIDE_UNIMPLEMENTED_FUNCTION` with a call count of two and an issue line of `2/0/0/0/1/0`.
**Of the forty-one ordinals that run calls, `grDrawPoint` was the only unimplemented one** —
and reading the code showed only two of nine draw entry points actually drawing, with the
other seven accepting requests and discarding them.

## 3. Change

`kMaxGlidePolygonVertices` in `glide_vertex.h`; `DrawPoint` (one vertex, `GL_POINTS`) and
`DrawPolygon` (n vertices, `GL_TRIANGLE_FAN`) on the backend; and boundary cases for point and
AA point, AA line, AA triangle and the six polygon forms. No new rendering path was needed
because `DrawPrimitive` is already generic. **Two judgements are recorded**: the AA variants
ignore their antialiasing flags and draw the same geometry, an approximation but plainly
closer than nothing; and the triangle fan is **exact**, not an approximation, because Glide
requires a convex polygon. **The ABI is unchanged** — two slots for `@4`, three for `@8`, four
for `@12`, seven for `@24`.

## 4. What passed

Both probes pass, the `unimplemented` counter reads `0/0/0/0/0/0` with drawing enabled against
`1/0/0/0/1/0` with it disabled, and no new decline reason appears.

## 5. The stall, and the evidence that it is not this change

Runs stopped at 13-33 seconds instead of 60, ended by `PollThreadUntilExit`'s **one-second
no-progress watchdog** rather than the timeout. An A/B on `REPIU_GLIDE_DRAW_ENTRY_POINTS`
inside one binary settles it: disabled gives 403/446/419 frames stopping at 12.9-13.5 s, and
enabled gives 427/430/449 stopping at 13.4-13.6 s — **indistinguishable**. Also excluded: the
Task 419 spin (`spin=0` stalls too, at 20.0 and 27.0 s), the EEPROM (the fixture is
**byte-identical** to the Task 418 and 419 copies; only its mtime moved), leftover processes
(none), and the `grDrawPoint` call count — **the run that drew two points ran twice as long**,
refuting the first hypothesis, since the count is a consequence of duration rather than a
cause.

**The stall is "further", not "shorter".** The 60-second Task 419 runs reach **ten** DOS path
traces ending at `bga\16.dat`; the 13-second runs reach **sixteen to nineteen**, through
`title\t301-t305.ptx`, `.\step\mix4_1.NOT` and `bga\30.dat` — the attract-mode demo, a point
the earlier runs never reached inside sixty seconds. **Why it now arrives there is
unresolved** (guest-time pacing, input, or environment) and is filed as its own frontier item.

## 6. What was not completed

The work order's **pumpit3 frame-regression check** could not be run: the stall makes a
60-second sample unobtainable. The within-condition A/B showing drawing on and off to be
indistinguishable is the strongest available substitute, and the regression check must be
repeated once the stall is understood.

## 7. Retrospective

The A/B switch again did the deciding, overturning a plausible story — the log named an API,
the API was implemented, the runs began stalling — with a single measurement. Distrusting the
first correlation was right: only runs with `drawpoint=1` appeared to stall until a run with
`drawpoint=2` ran longer, showing the count to be an effect of duration. And reading "it
stopped" as "it got further" rather than "it got less far" is what located the stall at all;
without comparing DOS trace counts the search would have stayed near boot.
