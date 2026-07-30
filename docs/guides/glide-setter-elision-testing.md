# Glide setter 생략 기본값 검증 가이드 / Testing the Glide setter elision default

이 문서는 Task 365가 기본 ON으로 넣은 **동일 Glide 상태 생략**을 사용자가 직접 실제
플레이 장면에서 검증하고, 기본값을 유지할지 opt-in으로 되돌릴지 판단하기 위한 절차입니다.

* 설계: [20260730-365-glide-setter-state-elision.md](../design/20260730-365-glide-setter-state-elision.md)
* 측정 결과: [작업 로그](../work-logs/20260730-365-glide-setter-state-elision.md)

## 왜 사용자 검증이 필요한가

Task 365의 자동 60초 측정은 **부팅부터 시작하는 장면**이라 `grLfbLock` 구간을 포함하고,
그 장면에서는 결론이 이렇게 나왔습니다.

| 항목 | 결과 |
|---|---|
| 정확성 | **증명됨** — 관측된 중복만 생략, 렌더 시퀀스 동일 |
| Glide gate 비용 | **-5.13%p** (rendezvous 41,368회 제거) |
| 프레임 | **변화 없음** (1,215 → 1,206, 편차 내) |

즉 **비용은 확실히 줄었지만 이 장면에서는 프레임으로 환산되지 않았습니다.** 반면
Task 363이 기록한 "호출량이 늘면 FPS가 급락하는" 실제 플레이 장면에서는 상태 setter가
Glide gate의 85.33%, wall-clock의 20.59%였습니다. 그 장면에서는 결과가 다를 수 있으나
아직 측정되지 않았습니다. **그 장면을 잡을 수 있는 사람은 사용자뿐입니다.**

## 1단계 — 두 구성으로 같은 구간을 플레이

FPS가 실제로 떨어지는 구간을 두 번, 가능하면 같은 곡·같은 난이도·같은 길이로
플레이합니다.

```
:: 생략 ON (현재 기본값)
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIME_PROFILE=1
set REPIU_GLIDE_ORDINAL_TIME_PROFILE=1
set REPIU_GLIDE_SETTER_CENSUS=1
set REPIU_GLIDE_SETTER_ELIDE=1
build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit1 2> elide-on.log
```

```
:: 생략 OFF (기존 동작)
set REPIU_GLIDE_SETTER_ELIDE=0
build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit1 2> elide-off.log
```

나머지 환경 변수는 두 실행에서 **완전히 같아야** 합니다. 로그는 stderr로 나오므로 위처럼
`2>`로 받습니다.

**주의:** 실행 간 프레임 편차가 18%까지 관측됐고 각 구성의 첫 실행이 늘 가장 느립니다
(Task 335). 따라서 **각 구성 3회 이상** 하고 중앙값을 쓰십시오. 1회 비교로는 판단할 수
없습니다.

## 2단계 — 로그에서 볼 숫자

각 로그 끝에서 다음 네 줄을 찾습니다.

| 찾을 문자열 | 읽을 값 |
|---|---|
| `Win32 Glide call trace: ordinal=... _GRBUFFERSWAP@4 count=` | **프레임 수** |
| `Win32 execution time share veh/glide-gate/...` | **glide-gate 비중** |
| `Win32 Glide setter elision enabled/entries/elided/applied/...` | **생략 횟수** |
| `Win32 Glide setter census enabled/entries/calls/first/same/changed/...` | **호출 수와 동일 상태 수** |

PowerShell로 한 번에 뽑는 방법입니다.

```powershell
foreach ($f in @("elide-off.log", "elide-on.log")) {
    "=== $f ==="
    Select-String -Path $f -Pattern `
        "_GRBUFFERSWAP@4 count=|execution time share|setter elision enabled|setter census enabled" |
        ForEach-Object { $_.Line }
}
```

## 3단계 — 판정

### 정확성 (이게 먼저입니다)

**census `same` 과 elision `elided` 가 같아야 합니다.** census는 동작을 바꾸지 않는
관측자이므로, 두 값이 같다는 것은 확인된 중복만 생략했다는 뜻입니다.

ON 로그에서 7종 setter의 `same` 합계와 `elided` 총합을 비교하십시오. 다르면
**즉시 `REPIU_GLIDE_SETTER_ELIDE=0`으로 되돌리고 로그를 첨부해 알려주십시오.** 자동
측정에서는 3회 모두 정확히 일치했지만, 다른 장면에서 어긋난다면 규칙에 빠진 경계가
있다는 뜻입니다.

`voided` 값도 확인하십시오. 자동 측정에서는 0이었습니다. 0이 아니면 backend 실패가
발생한 것이므로 함께 알려주십시오.

### 시각

같은 구간을 눈으로 비교하십시오. 특히 다음을 보십시오.

* 반투명(블렌딩) 요소 — 화살표 잔상, 페이드, 콤보 이펙트
* 화면 잘림 — `grClipWindow` 생략이 잘못되면 경계가 어긋납니다
* 색 채널 이상 — `grColorMask` 생략이 잘못되면 채널이 빠집니다
* 앞뒤 겹침 순서 — `grDepthBufferFunction` 생략이 잘못되면 뒤에 있어야 할 것이 앞에 옵니다

이상이 보이면 되돌리고 알려주십시오. 자동 측정에서는 렌더 시퀀스가 phase offset +1에서
72.9% 완전 일치했으므로(같은 프레임을 한 프레임 먼저 그림) 차이가 나올 가능성은 낮습니다.

### 성능

| 관측 | 의미 | 권고 |
|---|---|---|
| 프레임 중앙값 ON > OFF, 차이 5% 초과 | 이 장면은 setter 경로에 제한돼 있었음 | **기본 ON 유지.** batch 2 진행 가치 있음 |
| 프레임 차이 5% 이내 | 이 장면도 setter 경로에 제한되지 않음 | 기본값 결정은 취향. batch 2는 중단 |
| 프레임 중앙값 ON < OFF, 5% 초과 | 예상 밖 — 조사 필요 | 되돌리고 로그 첨부 |

glide-gate 비중은 두 경우 모두 내려갈 것입니다(비용은 확실히 줄어듦). 판단 기준은
**프레임**입니다.

## 4단계 — 함께 남기면 좋은 자료

`INT 8 chain HLE count` 줄도 함께 뽑아 주시면 Task 366(pacing 귀속)에 직접 쓸 수
있습니다. 실제 플레이 장면의 timer tick 전달률이 부팅 장면과 같은지가 현재 열린
질문입니다.

```powershell
Select-String -Path elide-on.log -Pattern "INT 8 chain HLE count" |
    ForEach-Object { $_.Line }
```

---

## English

This guide lets you validate the exact-state Glide setter elision that Task 365
enabled by default, in a real gameplay scene, and decide whether the default
should stay on.

**Why your test matters.** The automated 60-second measurement starts from boot
and includes the LFB section. There, correctness was proven and the Glide gate
cost fell 5.13 points (41,368 rendezvous removed), but frames did not move
(1,215 to 1,206, inside run variance). In the gameplay scene behind the original
"FPS collapses when call volume rises" report, Task 363 measured state setters at
85.33% of the Glide gate and 20.59% of wall time, so the outcome could differ —
and only you can capture that scene.

**Procedure.** Play the same section twice with identical settings, once with
`REPIU_GLIDE_SETTER_ELIDE=1` and once with `=0`, keeping
`REPIU_EXECUTION_BACKEND=aot-dbt`, `REPIU_EXECUTION_TIME_PROFILE=1`,
`REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`, and `REPIU_GLIDE_SETTER_CENSUS=1` the same
in both, capturing stderr to a file. Run each configuration at least three times
and use medians: run-to-run frame variance reaches 18% and the first run of a
configuration is always the slowest.

**Read four lines** from each log: the `_GRBUFFERSWAP@4 count=` frame count, the
`execution time share` glide-gate percentage, the `setter elision` summary, and
the `setter census` summary.

**Judge correctness first.** The census is a behaviour-neutral observer, so its
`same` count must equal the elision's `elided` count — that is what proves only
confirmed duplicates were skipped. If they differ, or if `voided` is nonzero, set
`REPIU_GLIDE_SETTER_ELIDE=0` and send the log: it means the rules are missing a
boundary that this scene exercises. Then compare the two runs visually, watching
translucent effects, screen clipping, color channels, and front-to-back ordering,
since those are the states being cached.

**Then judge performance.** If median frames improve by more than 5%, the scene
was limited by this path: keep the default and batch two is worth doing. Within
5%, this scene is not limited by it either, batch two stops, and the default
becomes a preference. A regression beyond 5% is unexpected and worth reporting.
The glide-gate share will fall either way, since the cost reduction is real; the
deciding number is frames.

**Also useful:** capture the `INT 8 chain HLE count` line. Whether a real gameplay
scene delivers timer ticks at the same rate as the boot scene is an open question
that Task 366 is working on, and your log would feed straight into it.
