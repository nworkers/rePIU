# Task 515 작업 로그 — 초과 경계의 종류

설계: [20260828-515](../design/20260828-515-boundary-class-excess.md) ·
작업 지시: [20260828-515](../work-orders/20260828-515-boundary-class-excess.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
측정 절차: [execution-frame-rate-measurement](../guides/execution-frame-rate-measurement.md)

## 결과 — 초과분은 breakpoint입니다

세 클래스 합이 `veh_count`와 정확히 맞습니다(623,842 대 623,843 — 진행 중인 하나 차이).
**프레임당 배달 수**로 비교합니다.

| 클래스 | Windows | Linux | 배율 | 초과분 기여 |
|---|---:|---:|---:|---:|
| single-step | 4.87 | 8.95 | 1.8x | 1.8% |
| **breakpoint** | 6.94 | **167.80** | **24.2x** | **69%** |
| other (접근 위반) | 6.92 | 74.29 | 10.7x | 29% |
| 합계 | 18.2 | 250.6 | 12.6x | |

배달 중 비율로도 성격이 뒤집혀 있습니다 — Windows는 single-step 24.4% / breakpoint 38.5% /
other 37.1%로 고른데, Linux는 **breakpoint 67.0%**에 몰려 있고 single-step은 3.6%뿐입니다.

## 유력 후보 둘을 대조군으로 반증했습니다

설계는 frontier 6절의 후보를 지목했습니다 — 하드웨어 디버그 레지스터 부재로 차단된 세 native
경로. **breakpoint가 지배적으로 나온 것은 그 후보와 일치**하지만, 일치는 시험이 아닙니다.

**먼저 정제가 하나 나왔습니다.** `REPIU_NATIVE_REGION`과 `REPIU_NATIVE_LINEAR_SPAN`은
Windows에서도 **opt-in**이라 기본으로 꺼져 있습니다. **두 호스트의 기본값이 다른 것은
`native_fast_path` 하나뿐**입니다. 그래서 시험이 정확해집니다.

| Windows 대조군 | bp/frame | 기준선 대비 |
|---|---:|---:|
| 기준선 | 6.94 | — |
| `REPIU_DISABLE_NATIVE_FAST_PATH=1` | 6.67 | **−3.9%** |
| `REPIU_AOT_DIRECT_RETURN_TABLE=0` | 6.73 | **−2.9%** |

**둘 다 움직이지 않습니다.**

### 그리고 첫 번째 반증은 읽는 방식이 중요했습니다

`native_fast_path`를 껐는데 변화가 없었습니다. 510이 기록한 함정이 바로 이 자리입니다 —
"축이 아니다"와 "노브가 아무 일도 안 했다"가 같은 모양입니다. 그래서 카운터를 봤습니다.

```
Win32 native fast path entry/return/cancel: 0/0/0
```

**기준선 실행에서도 0/0/0입니다.** 그 경로는 이 장면에서 **Windows에서도 한 번도 작동하지
않습니다.** 끄나 켜나 같은 것이 당연하고, 동시에 **두 호스트의 차이가 될 수 없다**는 것이
확정됩니다. 카운터를 보지 않았으면 "꺼도 안 변하니 원인이 아니다"라는 약한 결론에 그쳤을
것을, 보고 나니 **더 강한 결론**이 됐습니다.

두 번째 반증은 노브가 실제로 작동했음을 확인했습니다 —
`AOT direct-return table enabled/sites/entries/hits/...: false/0/0/0/0.00%/...`. 즉 진짜로
꺼졌는데도 breakpoint 수는 그대로입니다. **direct-return table은 host dispatch를 없애는
장치이지 트랩을 없애는 장치가 아니었습니다.**

## 구현

`live_execution_profile_report.cpp`의 `[repiu-live-veh]` 줄에 나머지 두 클래스를 더했습니다.
**새로 세는 것은 없습니다** — `veh_gap_counts`와 `veh_gap_cycles`는 세 칸 다 채워지고 있었고
512가 한 칸만 찍었습니다. 512와 같은 모양의 변경입니다.

## 검증

| 항목 | 결과 |
|---|---|
| Linux Release 빌드 | 성공 |
| **세 클래스 합** | `veh_count`와 일치 (623,842 / 623,843) |
| Linux 실행 | 3회, 구성 재현 (bp 66.8~67.0%) |
| Windows Release 빌드 | 성공 |
| **새 줄 대 요약** | ss/bp/other 243,701/385,919/377,094 (보고 #8, 약 80초) 대 최종 250,179/396,073/395,097 — 누적으로 일관 |
| 대조군 2건 | 노브가 실제로 작동했는지 카운터로 확인 |

## 남은 경계 — 다음 후보

**무엇이 프레임당 168개의 INT3을 만드는지는 아직 모릅니다.** 반증된 둘은 지웠고, 다음 후보는
**AOT 코드 캐시가 Linux에서 경계를 얼마나 이어 붙이는가**입니다 — direct edge 연결과 인라인
패치가 Windows만큼 성립하는지. Task 506이 "인라인 패치 동작"까지는 확인했지만 **얼마나
효과적인지는 재지 않았습니다.**

그 수치(patched/verified/resolved-target/fallback 계열)는 지금 `main.cpp` 요약으로만 나가므로
**Linux에서 읽을 수 없습니다.** 512·515와 같은 모양으로 live 줄에 실으면 됩니다.

`other`(접근 위반)가 10.7배인 것도 남습니다 — 페이지 보호·포트 I/O·write watch 쪽입니다.

---

# Task 515 work log — what the excess boundaries are

Design: [20260828-515](../design/20260828-515-boundary-class-excess.md) ·
Work order: [20260828-515](../work-orders/20260828-515-boundary-class-excess.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Procedure: [execution-frame-rate-measurement](../guides/execution-frame-rate-measurement.md)

## Result — the excess is breakpoints

The three classes sum exactly to `veh_count` (623,842 against 623,843 -- the one in flight). The
comparison is **deliveries per frame**.

| Class | Windows | Linux | Factor | Share of the excess |
|---|---:|---:|---:|---:|
| Single-step | 4.87 | 8.95 | 1.8x | 1.8% |
| **Breakpoint** | 6.94 | **167.80** | **24.2x** | **69%** |
| Other (access violations) | 6.92 | 74.29 | 10.7x | 29% |
| Total | 18.2 | 250.6 | 12.6x | |

The character is inverted in share as well: Windows is spread evenly at 24.4% single-step, 38.5%
breakpoint and 37.1% other, while Linux is concentrated at **67.0% breakpoint** with single-step at
just 3.6%.

## Two leading candidates refuted by control

The design pointed at the frontier's candidate -- the three native paths blocked for want of
hardware debug registers. **Breakpoints dominating is consistent with it**, but consistency is not a
test.

**A refinement came first.** `REPIU_NATIVE_REGION` and `REPIU_NATIVE_LINEAR_SPAN` are **opt-in on
Windows too**, so they are off there by default. **The only default that differs between the hosts is
`native_fast_path`** -- which makes the test exact.

| Windows control | bp/frame | vs baseline |
|---|---:|---:|
| Baseline | 6.94 | — |
| `REPIU_DISABLE_NATIVE_FAST_PATH=1` | 6.67 | **−3.9%** |
| `REPIU_AOT_DIRECT_RETURN_TABLE=0` | 6.73 | **−2.9%** |

**Neither moves.**

### And how the first refutation was read mattered

Turning `native_fast_path` off changed nothing. That is exactly the trap Task 510 recorded -- "not
the axis" and "the knob did nothing" look the same. So the counter was checked:

```
Win32 native fast path entry/return/cancel: 0/0/0
```

**The baseline run reports 0/0/0 as well.** That path **never engages in this scene on Windows
either.** Off and on being identical is then expected, and at the same time it settles that it
**cannot be the difference between the hosts.** Without looking at the counter this would have
stopped at the weak "turning it off changes nothing"; with it, the conclusion is **stronger**.

The second refutation confirmed its knob did act --
`AOT direct-return table enabled/sites/entries/hits/...: false/0/0/0/0.00%/...`. It genuinely turned
off and the breakpoint count still did not move. **The direct-return table removes host dispatches,
not traps.**

## Implementation

The remaining two classes were added to the `[repiu-live-veh]` line in
`live_execution_profile_report.cpp`. **Nothing new is counted** -- `veh_gap_counts` and
`veh_gap_cycles` were filling all three slots and 512 printed one. The same shape of change as 512.

## Verification

| Item | Result |
|---|---|
| Linux Release build | succeeded |
| **The three classes sum** | matches `veh_count` (623,842 / 623,843) |
| Linux runs | three, composition reproduced (bp 66.8-67.0%) |
| Windows Release build | succeeded |
| **The new line against the summary** | ss/bp/other 243,701/385,919/377,094 (report #8, about 80 s) against a final 250,179/396,073/395,097 -- consistent as cumulative |
| Two controls | the knob was confirmed to have acted, by counter |

## Remaining boundary — the next candidate

**What produces 168 INT3s a frame is still unknown.** Two candidates are struck off, and the next is
**how well the AOT code cache links its boundaries on Linux** -- whether direct-edge linking and
inline patching hold as they do on Windows. Task 506 confirmed inline patching *runs*; **how
effective it is was never measured.**

Those numbers (the patched / verified / resolved-target / fallback family) currently leave only
through `main.cpp`'s summary, so they **cannot be read on Linux**. Putting them on the live line is
the same shape of change as 512 and 515.

`other` (access violations) at 10.7x also remains -- that is page protection, port I/O and write
watches.
