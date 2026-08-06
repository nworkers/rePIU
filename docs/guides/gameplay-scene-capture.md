# FPS 급락 gameplay 장면 캡처 가이드 / Capturing the FPS-collapse gameplay scene

이 문서는 **실제로 FPS가 떨어지는 gameplay 장면**의 profile을 남기는 절차입니다.
Tasks 364~368이 모두 부팅 포함 자동 60초 장면에서 측정됐고 세 번 연속 "이 장면에서는
이득이 작다"가 나왔는데, **정작 문제가 보고된 장면은 아직 한 번도 측정되지
않았습니다.** 이 캡처가 다음 대상 선정의 가장 큰 정보입니다.

## 왜 장면이 결정적인가

| 장면 | 상태 setter의 wall 비중 | `grLfbLock` | 출처 |
|---|---:|---:|---|
| Task 363 gameplay (문제 발생) | **20.59%** | **0회** | 수동 플레이 47.5초 |
| Tasks 364~368 자동 실행 | 약 5.6% | 304회 | 부팅 포함 60초 |

**같은 setter 집합이 장면에 따라 4배 차이 납니다.** 자동 실행은 `grLfbLock`이 Glide
gate를 지배해 setter 비중을 희석시킵니다. Task 364가 잰 생략 상한도 Glide gate 대비
25.11%인데 wall 대비로는 4.55%로 떨어지는 이유가 이것입니다.

## 1. 실행

FPS가 실제로 떨어지는 구간을 플레이합니다. **곡 선택 화면이 아니라 노트가 쏟아지는
구간**이어야 합니다.

```
set REPIU_EXECUTION_BACKEND=dynamic
set REPIU_EXECUTION_TIMEOUT_MS=0
set REPIU_GLIDE_SWAP_INTERVAL=0
set REPIU_EXECUTION_TIME_PROFILE=1
set REPIU_GLIDE_ORDINAL_TIME_PROFILE=1
set REPIU_GLIDE_SETTER_CENSUS=1
build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit1 2> gameplay-capture.log
```

> **`REPIU_GLIDE_SWAP_INTERVAL=0`은 측정의 필수 조건입니다(Task 371).** 기본 구성은
> vsync라서 present가 프레임당 약 2.77 ms를 **유휴 대기**로 쓰고, 그 시간이
> `grBufferSwap`의 gate 시간으로 계상돼 Glide 비중을 부풀립니다. 같은 장면에서 Glide
> gate가 vsync 20.20% / interval 0 12.52%로 갈렸습니다.
>
> 더 중요한 것은 **vsync 구성에서는 CPU 개선이 프레임에 나타나지 않는다**는 점입니다.
> double buffer + 60 Hz에서 마감을 놓치면 30 fps로 양자화되므로 CPU를 20.3 → 17 ms로
> 줄여도 fps는 30 그대로이고, 16.7 ms 아래로 내려가야 60으로 점프합니다. interval 0
> 에서는 개선이 프레임에 비례해 나타나므로 A/B 판정이 가능합니다.
>
> **게임 플레이 기본값은 vsync 그대로입니다.** 이 변수는 측정할 때만 씁니다.

`REPIU_EXECUTION_TIMEOUT_MS`는 **0(무제한)이어야 합니다.** Task 435부터 미설정
기본값도 0이지만, 캡처 절차는 다른 값이 남아 있는 셸에서도 같은 조건이 되도록
그대로 명시합니다. **종료 요약이 남으려면 정상 종료해야** 하므로 강제
종료하지 말고 창을 닫으십시오(X 또는 Alt+F4). 창 닫기는 `SDL_EVENT_QUIT` 경로로
timeout과 같은 teardown을 타므로 요약이 동일하게 남습니다.

> 20초 정도의 짧은 실행은 요약 없이 exit 255로 끝나는 것이 관측됐습니다(계측과 무관).
> **60초 이상**을 권장합니다.

**주의:** 실행 간 프레임 편차가 18%까지 관측됩니다. 가능하면 같은 구간을 **3회**
남겨 주십시오.

## 2. 캡처에서 볼 값

```powershell
Select-String -Path gameplay-capture.log -Pattern `
  "_GRBUFFERSWAP@4 count=|execution time share|execution time cycles guest-run|" +
  "Glide gate entries/handled|setter census enabled|setter elision enabled|" +
  "exception census|Glide gate prologue|boundary effective opcodes|" +
  "boundary 0F escape|timer tick delivery|INT 8 chain HLE count" |
  ForEach-Object { $_.Line }
```

또한 ordinal별 Glide 비용이 다음 대상 선정의 핵심이므로 함께 남겨 주십시오.

```powershell
Select-String -Path gameplay-capture.log -Pattern "Glide ordinal timing: ordinal=" |
  ForEach-Object { $_.Line } | Select-Object -Last 40
```

## 3. 이 캡처로 답할 질문

1. **이 장면에서 Glide gate는 wall의 몇 %이고, ordinal별로 무엇이 지배하는가?**
   자동 장면에서는 gate 본체가 호출당 약 235,000 cycle로 최대 덩어리였고 `grLfbLock`이
   유력했습니다. LFB가 없는 장면에서는 다른 답이 나올 것입니다.
2. **Task 365 setter 생략이 여기서는 프레임을 늘리는가?**
   기본 ON이므로 이미 켜져 있습니다. `REPIU_GLIDE_SETTER_ELIDE=0`과 비교하면
   판정됩니다. 절차는 [생략 검증 가이드](glide-setter-elision-testing.md)에 있습니다.
3. **프레임당 예외와 그 구성이 자동 장면과 같은가?**
   자동 장면은 프레임당 325개, 그중 Glide gate trap이 55.21%였습니다.
4. **timer tick 전달률이 여기서도 88% 수준인가?**

## 4. 캡처 뒤 결정 트리

| 관측 | 다음 대상 |
|---|---|
| Glide gate가 wall의 20%+ 이고 setter가 그 대부분 | Task 365 batch 2 재개, 생략 기본값 유지 |
| Glide gate가 크지만 LFB/triangle이 지배 | 해당 ordinal 분해(Task 356 또는 366 재개) |
| Glide gate가 작고 guest 실행이 지배 | Glide 축을 닫고 guest 실행 내부로 이동 |
| 프레임당 예외가 자동 장면보다 훨씬 큼 | 예외 축을 재개(Task 368의 종결 판정 재검토) |

---

## English

This is the procedure for profiling **the gameplay scene where FPS actually
collapses**. Tasks 364 through 368 were all measured on a boot-inclusive automated
60-second scene and produced "the gain is small here" three times running, while the
scene the problem was reported from has never been measured. This capture is the
single largest input to choosing what comes next.

**Why the scene decides it:** the same state-setter set held 20.59% of wall time in
the Task 363 gameplay capture with zero `grLfbLock` calls, against about 5.6% in the
automated runs where 304 LFB locks dominate the Glide gate. That is a fourfold
difference from scene composition alone, and it is why Task 364's elision ceiling
reads 25.11% against the Glide gate but only 4.55% against wall time.

Play a section where the drop is real — notes falling, not a menu — with
`REPIU_EXECUTION_BACKEND=dynamic`, `REPIU_EXECUTION_TIMEOUT_MS=0`,
`REPIU_EXECUTION_TIME_PROFILE=1`, `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`, and
`REPIU_GLIDE_SETTER_CENSUS=1`, capturing stderr. Both of the first two are also the
defaults since Task 435, but the procedure still states them so a shell carrying
other values produces the same run. The run must end normally or by timeout for the exit summary to be written,
so do not kill it; runs of about twenty seconds have been observed exiting 255
without a summary, so prefer sixty seconds or more. Run-to-run frame variance
reaches 18%, so three captures of the same section are much better than one.

Collect the frame count, execution time shares and cycles, Glide gate entries, the
setter census and elision summaries, the exception census, the Glide gate prologue,
the boundary opcode lists, timer tick delivery, and the `INT 8` chain count — plus
the per-ordinal Glide timing lines, which are what identify the dominant cost.

The capture answers four questions: what share the Glide gate holds and which
ordinal dominates it here; whether Task 365's setter elision moves frames in this
scene (compare against `REPIU_GLIDE_SETTER_ELIDE=0`, procedure in the
[elision testing guide](glide-setter-elision-testing.md)); whether exceptions per
frame and their composition match the automated scene's 325 per frame with the Glide
gate trap at 55.21%; and whether tick delivery is also near 88% here.

The decision tree: a large Glide gate dominated by setters reopens batch two and
settles the elision default; a large gate dominated by LFB or triangles points at
decomposing that ordinal; a small gate with guest execution dominant closes the Glide
axis entirely; and exceptions per frame far above the automated scene would reopen
the exception axis that Task 368 closed.
