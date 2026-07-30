# 20260730-365 동일 Glide 상태 생략 작업 로그 / Work log

* 설계: [20260730-365-glide-setter-state-elision.md](../design/20260730-365-glide-setter-state-elision.md)
* 작업 지시: [20260730-365-glide-setter-state-elision.md](../work-orders/20260730-365-glide-setter-state-elision.md)
* 근거 측정: [Task 364](20260730-364-glide-setter-state-census.md)
* 측정 산출물: `build/benchmarks/glide-setter-elision/20260730-144642/` (로컬, Git 제외)

## 한국어

### 결론 요약

**정확성은 이례적으로 높은 수준으로 증명됐고, 비용은 확실히 제거됐지만, 이 장면에서
프레임 개선은 확인되지 않았습니다.** 사전 등록 판정 **P3**입니다.

| 항목 | elide OFF | elide ON | 판정 |
|---|---:|---:|---|
| 프레임 중앙값 | 1,215 | 1,206 | **-0.74% (편차 내, 개선 없음)** |
| 프레임 3회 범위 | 1,215~1,384 | 1,194~1,207 | OFF 편차 13.9% |
| Glide gate 비중 중앙값 | 20.76% | **15.63%** | -5.13%p |
| Glide gate 3회 범위 | 16.55~20.98% | 15.27~15.71% | ON이 훨씬 안정 |
| 생략된 rendezvous | 0 | **41,368** | 계수값 |
| 실제 host 적용 | — | **16** | — |

### 이 작업의 한 줄 사실

**게임은 60초 동안 이 7종 setter를 41,384회 호출해 상태를 실제로 16번 바꿉니다.**
비율 약 **2,586 : 1**입니다. `applied=16`은 3회 실행에서 모두 정확히 16이었습니다.

### E1 — 정확성의 핵심 증명

census는 동작을 바꾸지 않는 순수 관측자입니다. 그것이 독립적으로 "정확한 중복"이라고
센 횟수와 실제로 생략한 횟수가 **ordinal 단위로 정확히 일치**했습니다.

| ordinal | API | calls | census `same` | cache `elided` |
|---:|---|---:|---:|---:|
| 91 | `grColorMask` | 7,461 | 7,458 | **7,458** |
| 101 | `grFogMode` | 5,663 | 5,661 | **5,661** |
| 82 | `grAlphaTestFunction` | 5,656 | 5,654 | **5,654** |
| 89 | `grClipWindow` | 5,656 | 5,654 | **5,654** |
| 94 | `grCullMode` | 5,656 | 5,654 | **5,654** |
| 79 | `grAlphaBlendFunction` | 5,656 | 5,653 | **5,653** |
| 96 | `grDepthBufferFunction` | 5,656 | 5,653 | **5,653** |

3회 실행 모두 합계도 정확히 일치했습니다(41,368/41,368, 41,188/41,188,
41,506/41,506). **관측된 중복만 생략했고 그 외에는 하나도 생략하지 않았습니다.**
`voided` 0, `failure` 0, `unsupported` 0입니다.

### E7 — 렌더 시퀀스 동일성 (설계 정정 포함)

**설계 정정:** 초기 설계는 E7을 `REPIU_GLIDE_FRAME_DUMP` BMP 육안 비교로 잡았으나,
그 변수는 back buffer 이미지가 아니라 **draw-call 추적**입니다. back buffer
스크린샷 기능은 현재 없습니다. 설계 문서의 E7을 시퀀스 동일성 검사로 정정했습니다.

`REPIU_GLIDE_PIXEL_DIAG`가 swap 번호별로 남기는 non-black 픽셀 수와 채널 평균을
OFF/ON에서 phase offset을 주고 대응시켰습니다.

| phase offset | 비교 표본 | 통계 완전 일치 | 비율 |
|---:|---:|---:|---:|
| -2 | 58 | 0 | 0.0% |
| -1 | 59 | 1 | 1.7% |
| 0 | 63 | 10 | 15.9% |
| **+1** | 59 | **43** | **72.9%** |
| +2 | 58 | 9 | 15.5% |

**offset +1에서 72.9%가 완전 일치합니다.** non-black 픽셀 수와 R/G/B 세 채널 평균이
모두 정확히 같다는 뜻입니다. 예를 들어 OFF swap 12 `68113 rgb=36,22,0`이 ON swap 11과
동일하고, OFF 13 `68221 rgb=39,24,0`이 ON 12와 동일합니다.

**해석: 같은 프레임을 같은 순서로 그리고, 한 프레임 먼저 도달합니다.** mask나 blend가
깨졌다면 어떤 offset에서도 일치하지 않습니다. 집계 평균 비교(E6: non-black 21.60% 대
21.58%, -0.02%p)보다 훨씬 강한 검사입니다.

잔여 27.1%는 두 실행의 진행 속도가 조금씩 달라 offset이 구간마다 어긋난 결과이며,
어떤 구간에서도 채널이 계통적으로 틀어지지 않았습니다.

### 성능 — 정직한 판정

**비용은 제거됐습니다.** 41,368회 rendezvous 제거는 통계적 추론이 아니라 계수값이고,
Glide gate 비중이 20.76% → 15.63%로 5.13%p 내려간 것이 이를 확인합니다. ON 3회
(15.27/15.71/15.63%)가 OFF 3회(16.55/20.98/20.76%) 전부보다 낮아 완전 분리입니다.

**그러나 프레임은 늘지 않았습니다.** 1,215 → 1,206(-0.74%)이고 OFF 3회 범위가
1,215~1,384(13.9%)이므로 이 차이는 편차 안에 있습니다. **개선을 주장할 수 없습니다.**

이는 Task 335가 남긴 질문의 재현입니다. 그때는 비용 -3.53%p에 프레임 +5.5%였는데,
이번에는 비용 -5.13%p에 프레임 변화가 없습니다. **즉 이 장면의 실행은 더 이상 Glide
setter 경로에 의해 제한되지 않습니다.**

**정정 — 커널 전이 추정 상승은 측정 결과가 아니라 계측 artifact입니다.** 최초 기록에서
커널 전이 추정이 `7.26% → 10.08%`로 올랐다고 적었으나, 원인을 사후 확인한 결과
**두 구성이 서로 다른 calibration probe 값을 썼기** 때문입니다. Task 347 축은 세션마다
전이 가격을 새로 측정하는데, 두 호출의 값이 크게 달랐습니다.

| 구성 | `INT3` 가격 | single-step 가격 | 유도된 회당 가격 |
|---|---:|---:|---:|
| elide OFF | 28,154 | 30,838 | 29,050~29,079 |
| elide ON | **41,033** | **42,328** | **41,465~41,466** |

예외 **횟수**는 사실상 동일했습니다(OFF 401,898~449,330 대 ON 395,125~396,679,
프레임당 324.7~335.2 대 327.6~330.9). 즉 차이는 전량 **가격**이고 횟수는 아닙니다.
`guest_run_cycles`는 여섯 실행 모두 162.7~162.9G로 안정적이므로 분모 문제도 아닙니다.

**방법 규칙 추가:** 전이 가격 calibration은 세션 간 최대 46% 흔들립니다. 따라서 **서로
다른 task347 호출에서 나온 커널/guest 파생 축은 비교하지 않습니다.** 비교 가능한 것은
직접 측정값(Glide cycle, 예외 횟수, 프레임)입니다.

**정정된 귀속:** Glide gate는 중앙값 `33.81G → 25.46G cycle`로 8.35G(약 3.1초) 줄었고
예외 횟수는 늘지 않았으므로, 해방된 시간은 **AOT 캐시 내 guest 실행**으로 갔습니다.
그리고 timer safe-point trap이 프레임당 `4.80 → 5.25`(+9.4%)로 늘었습니다. 즉
**guest는 그 시간을 busy-wait에서 소비했습니다.**

**따라서 pacing 후보가 좁혀졌습니다(Task 366으로 이어짐).** 같은 실행에서 `INT 8`
전달은 198.5~208.5Hz인데 guest가 프로그램한 divisor는 4972 = **240Hz**입니다. 프레임당
tick은 6회 중 5회가 9.88~10.25로 좁고, tick rate가 가장 높은 실행(208.5Hz)이 프레임도
가장 높았습니다(1,384). 다만 240Hz 프로그래밍이 실행 시작 약 7초 뒤에 일어나므로 60초
평균은 부팅 구간에 끌려 내려갑니다. **정확한 결손률과 인과는 Task 366에서 측정합니다.**

**장면 조건:** 이번 자동 실행은 부팅부터 시작해 LFB 구간을 포함합니다. Task 363이
기록한 "호출량이 늘면 FPS가 급락하는" gameplay 장면은 setter가 Glide gate의 85.33%,
wall의 20.59%였으므로 이득이 훨씬 클 수 있으나 **그 장면은 아직 측정하지 않았습니다.**

### 검증

| gate | 결과 |
|---|---|
| E1 census `same` == cache `elided` | **통과 (ordinal 단위·합계 모두 정확히 일치, 3회)** |
| E2 호출 보존 (프레임당) | 통과 (34.35 대 34.43, +0.23%) |
| E3 ABI `completed <= handled`, liveness | 통과 |
| E4 malformed/fatal/issue/overflow/clamp = 0 | 통과 |
| E5 성능 | **미달 — 프레임 개선 없음.** Glide 비중은 -5.13%p |
| E6 back-buffer 집계 통계 | 통과 (non-black -0.02%p) |
| E7 렌더 시퀀스 동일성 | **통과 (offset +1에서 72.9% 완전 일치)** |
| E8 EEPROM 격리 | 통과 |

* `scripts/build_win32_x86.bat`, `scripts/build_win32_x86_release.bat`: 통과
* `repiu_aot_probe.exe`: 두 구성 exit 0, 신규 cache probe 12개 항목 전부 true
  (batch 1 목록이 공유 state-gate 목록의 부분집합인지 전수 검사 포함)
* `VERSION`: `0.0.113` 유지

### 구현 메모

* 규칙을 두 번 정의하지 않기 위해 key·동등성·gate 분류를
  `glide_setter_state_model.{h,cpp}`로 추출했고 census(관측자)와 cache(행위자)가 같은
  함수를 씁니다. boundary도 통합 scope 하나로 합쳐 key를 한 번만 만들고 결과를 한 번만
  분류합니다. **E1이 의미를 갖는 이유가 이 구조입니다.**
* 생략은 반환 주소·signature·인수 크기 검증을 모두 통과한 뒤에만 적용합니다.
* `glide_state` mirror 쓰기를 건너뛰어도 안전한 근거는 멱등성입니다. 이 mirror는
  `grGlideGetState`가 `BuildGlideStateImage`로 guest에 돌려주므로 실제로 읽히지만,
  key가 인수 dword 전체를 담으므로 직전 적용이 이미 동일한 값을 썼습니다.
* host 소유 `backend.message_`는 생략 경로에서 건드리지 않습니다(경합 회피).

### 다음 작업 제안

1. **기본값 결정이 필요합니다.** 현재 기본 ON입니다. 정확성은 증명됐고 비용은
   제거됐으나 이 장면 프레임 이득은 없습니다. 사용자의 gameplay 캡처 결과를 보고
   유지/opt-in 전환을 정하는 것이 합리적입니다.
2. **batch 2는 보류합니다(P3).** 이 장면이 setter 경로에 제한되지 않으므로
   `grDepthMask`·`grConstantColorValue`·texture 3종을 추가해도 같은 결과가 예상됩니다.
   이득이 큰 장면이 측정된 뒤 재개합니다.
3. **Task 366 triangle batching도 같은 이유로 보류 후보입니다.** 비용을 줄여도
   프레임이 늘지 않는 구간에 진입했다는 신호가 두 번(Task 335, 이번) 나왔습니다.
4. **다음 우선순위는 "무엇이 pacing하는가"입니다.** Task 367의 전체 축 재귀속을
   앞당겨, 비용 제거가 프레임으로 환산되지 않는 원인을 먼저 귀속하는 편이 낫습니다.

---

## English

### Result

**Correctness is proven to an unusually high standard and the cost is
demonstrably removed, but no frame improvement is shown in this scene.** The
pre-registered verdict is **P3**.

Median frames moved 1,215 to 1,206 (-0.74%) while the elide-off range was
1,215-1,384, so the frame difference sits inside run variance. The Glide gate
share fell from 20.76% to 15.63%, a 5.13-point drop, and all three elide-on
samples (15.27/15.71/15.63%) fall below all three elide-off samples
(16.55/20.98/20.76%). The removal itself is counted rather than inferred:
41,368 host rendezvous were skipped.

The single fact of this task: **the game issues 41,384 calls to these seven
setters in 60 seconds in order to actually change state 16 times** — a ratio of
about 2,586 to 1, with `applied` reading exactly 16 in all three runs.

### E1, the decisive correctness gate

The census is a pure observer that changes no behavior. What it independently
counted as an exact duplicate equalled what was actually skipped, **per ordinal
and in aggregate, in all three runs** — 7,458/7,458 for `grColorMask`,
5,661/5,661 for `grFogMode`, and so on, with run totals of 41,368/41,368,
41,188/41,188, and 41,506/41,506. Only observed duplicates were elided, and
nothing else was. Voided, failure, and unsupported counts were all zero.

### E7 and a design correction

**Correction:** the design specified E7 as a human comparison of
`REPIU_GLIDE_FRAME_DUMP` BMPs, but that variable is a draw-call trace, not a
back-buffer image dump, and no back-buffer screenshot facility exists. E7 is
replaced by a sequence-identity check, and the design document is updated.

Matching the per-swap back-buffer statistics under a phase offset gives 0.0% at
-2, 1.7% at -1, 15.9% at 0, **72.9% at +1**, and 15.5% at +2, where a match
means the non-black pixel count and all three channel means are exactly equal.
The elide-on run at swap N reproduces the elide-off run at swap N+1 — the same
frames in the same order, reached one frame sooner. A broken mask or blend would
match at no offset, which makes this far stronger than the aggregate comparison
in E6 (non-black 21.60% against 21.58%, a 0.02-point delta). The remaining 27.1%
is phase drift between two runs that progress at slightly different rates, with
no systematic channel shift anywhere.

### Performance, honestly

This reproduces the question Task 335 left open. There, a 3.53-point cost
reduction produced 5.5% more frames; here a 5.13-point reduction produces none.
Execution in this scene is therefore no longer limited by the Glide setter path.

**Correction:** the first version of this log reported the kernel-transition
estimate rising from 7.26% to 10.08%. That was an instrumentation artifact, not a
measurement. The Task 347 axis recalibrates the per-transition price every
session, and the two invocations disagreed sharply — `INT3` at 28,154 against
41,033 cycles and single-step at 30,838 against 42,328, a 46% spread — while the
exception *counts* were essentially identical (401,898-449,330 against
395,125-396,679, or 324.7-335.2 against 327.6-330.9 per frame). The difference
was entirely price and not count, and `guest_run_cycles` held at 162.7-162.9G in
all six runs, so it was not a denominator problem either. **Method rule: derived
kernel and guest shares from different task347 invocations are not comparable;
only directly measured quantities are.**

Corrected attribution: the Glide gate fell from a median 33.81G to 25.46G cycles,
about 3.1 seconds, and the exception count did not rise, so the freed time went
into guest execution inside the AOT cache — where timer safe-point traps rose
from 4.80 to 5.25 per frame, up 9.4%. The guest spent the time busy-waiting.

That narrows the pacing candidates, which Task 366 takes up. In the same runs
`INT 8` was delivered at 198.5-208.5 Hz while the guest programmed divisor 4972,
which is **240 Hz**. Ticks per frame sat in a tight 9.88-10.25 band in five of six
runs, and the run with the highest tick rate (208.5 Hz) also had the most frames
(1,384). The 240 Hz programming happens about seven seconds into the run, so the
60-second average is dragged down by the boot period; Task 366 measures the real
shortfall and whether the causation holds.

**Scene caveat:** this automated run starts from boot and includes the LFB
section. The gameplay scene behind the original "FPS collapses when call volume
rises" report held setters at 85.33% of the Glide gate and 20.59% of wall time,
where the gain could be much larger — but that scene has not been measured.

### Verification

E1 passed exactly, E2 held at 34.35 against 34.43 target calls per frame, E3 and
E4 passed, E6 and E7 passed, E8 held. **E5 was not met: there is no frame
improvement**, though the Glide share fell 5.13 points. Both builds pass and the
probe suite exits 0 in both configurations, including a new cache probe whose
twelve checks cover an exhaustive verification that the batch-one list is a
subset of the shared state-gate list, that cold, voided, and invalidated records
are never elided, and that a texture download breaks key equality. `VERSION`
stays `0.0.113`.

### Next

The default needs a decision: elision is on by default, correctness is proven and
cost is removed, but this scene shows no frame gain, so keeping it or switching to
opt-in is best decided against the user's own gameplay capture. Batch two is on
hold under P3, since a scene not limited by this path would show the same result
for `grDepthMask`, `grConstantColorValue`, and the texture setters. Task 366's
triangle batching is a hold candidate for the same reason — two independent
signals now say cost reduction is not converting into throughput. The higher
priority is attributing what does pace the run, which argues for bringing Task
367's whole-axis re-attribution forward.
