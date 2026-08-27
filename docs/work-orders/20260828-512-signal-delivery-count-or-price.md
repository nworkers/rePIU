# Task 512 작업 지시 — 시그널 전달: 횟수인가 단가인가

설계: [20260828-512](../design/20260828-512-signal-delivery-count-or-price.md) ·
작업 로그: [20260828-512](../work-logs/20260828-512-signal-delivery-count-or-price.md)

## 0. 새로 세지 마십시오

`counts`도 gap 배열도 **이미 채워지고 있습니다.** 511의 보고기가 찍지 않을 뿐입니다. 카운터를
새로 추가하고 있다면 잘못 가고 있는 것입니다.

## 1. 두 번째 줄로 내십시오

`live_execution_profile_report.cpp`. 기존 줄은 몫, 새 줄은 전달의 해부입니다.

```
[repiu-live-veh] #N veh_count= per_frame= cycles_per_veh= gap_min= gap_ss_mean= gap_ss_count=
```

한 줄에 합치지 마십시오 — 사람도 스크립트도 읽기 나빠집니다.

## 2. 첫 실행에서 0이 아닌지 눈으로 보십시오

**이 세션에서 두 번 걸린 함정입니다.** 코드를 읽어 "채워질 것"이라고 판단한 것과 채워지는 것은
다릅니다. `veh_count=0`이나 `gap_ss_count=0`이면 결론을 내지 말고 **왜 안 채워지는지부터**
보십시오.

## 3. Windows는 다시 재지 않아도 됩니다

511의 대조 실행 로그에 전부 있습니다.

| 항목 | 값 | 어디서 |
|---|---|---|
| `veh` 배달 총계 | 1,141,066 | `execution time count guest-run/veh/...` |
| 프레임 | 60,761 | `[repiu-shutdown] frames=` |
| `gap min` | 21,756 | `VEH gap min/max/clamped` |
| single-step gap 평균 | 28,185 | `VEH gap mean single-step/...` |

같은 코드로 한 번 더 돌려 새 줄이 Windows에서도 나오는지는 확인하되, **숫자는 위와 같은
자리에서 나와야 합니다.** 다르면 새 줄이 틀린 것입니다.

## 4. 판정

$$\frac{\text{cycles}}{\text{frame}} = \frac{\text{deliveries}}{\text{frame}} \times \frac{\text{cycles}}{\text{delivery}}$$

42.6배가 어느 인자에 있는지만 보고하십시오. **양쪽 다 크면 각각 몇 배인지 적으십시오** —
"둘 다"는 답이 아니라 분해입니다.

`gap`은 `cycles_per_veh`와 함께 읽으십시오. 전자는 커널 경로만, 후자는 핸들러 본문까지입니다.

## 5. 하지 마십시오

* **고치지 마십시오.** 512는 가르는 데까지입니다.
* 하위 버킷으로 내려가지 마십시오.
* 한 번 실행을 판정으로 쓰지 마십시오 — 3회입니다.

---

# Task 512 work order — signal delivery: a count problem or a price problem

Design: [20260828-512](../design/20260828-512-signal-delivery-count-or-price.md) ·
Work log: [20260828-512](../work-logs/20260828-512-signal-delivery-count-or-price.md)

## 0. Do not count anything new

Both `counts` and the gap arrays **already fill.** 511's reporter simply does not print them. If you
are adding a counter, you have gone the wrong way.

## 1. Emit a second line

In `live_execution_profile_report.cpp`. The existing line carries the shares; the new one carries the
anatomy of a delivery.

```
[repiu-live-veh] #N veh_count= per_frame= cycles_per_veh= gap_min= gap_ss_mean= gap_ss_count=
```

Do not merge them into one -- it reads worse for a person and for a script.

## 2. Look at the first run and check the values are not zero

**This session has been caught twice by exactly this.** Reading the code and concluding "it will
fill" is not the same as seeing it fill. If `veh_count=0` or `gap_ss_count=0`, draw no conclusion and
find out **why it is empty** first.

## 3. Windows does not need re-measuring

511's control log already has all of it:

| Item | Value | Where |
|---|---|---|
| Total `veh` deliveries | 1,141,066 | `execution time count guest-run/veh/...` |
| Frames | 60,761 | `[repiu-shutdown] frames=` |
| `gap min` | 21,756 | `VEH gap min/max/clamped` |
| Single-step gap mean | 28,185 | `VEH gap mean single-step/...` |

Do run it once with the same code to see the new line appear on Windows too, but **the numbers must
land where the ones above did.** If they do not, the new line is wrong.

## 4. The verdict

$$\frac{\text{cycles}}{\text{frame}} = \frac{\text{deliveries}}{\text{frame}} \times \frac{\text{cycles}}{\text{delivery}}$$

Report only which factor carries the 42.6x. **If both are large, say by how much each** -- "both" is
not an answer, it is a decomposition.

Read `gap` together with `cycles_per_veh`: the first sees only the kernel path, the second includes
the handler body.

## 5. Do not

* **Do not fix anything.** 512 goes as far as separating the two.
* Do not descend into the sub-buckets.
* Do not treat one run as a verdict -- three.
