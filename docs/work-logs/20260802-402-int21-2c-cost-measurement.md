# 20260802-402 INT 21h AH=2Ch 비용 측정 작업 로그 / AH=2Ch Cost Measurement Work Log

## 한국어

### 작업 요약

Task 401이 남긴 `INT 21h AH=2Ch` 비용 가설을 wall clock 대비로 측정해 **기각**했습니다.
측정이 지목한 실제 비용 중심은 포트 `0x02A8` 폴링이었습니다. 코드는 바꾸지 않았습니다.

### 측정 결과 (45초 `aot-dbt` 2회)

| 항목 | base1 | base2 |
|---|---:|---:|
| wall cycles | 177,711,788,710 | 166,469,707,454 |
| 프레임 | 1,026 (22.8 FPS) | 867 (19.3 FPS) |
| `AH=2Ch` 호출 | 263,832 | 208,740 |
| DOS 서비스 본체 | 0.146% | 0.047% |
| DOS 본체 + 커널 왕복 | **3.84%** | **3.19%** |
| port I/O 본체 | 30.53% | 24.16% |
| port I/O + 커널 왕복 | **56.39%** | **46.06%** |
| Glide gate | 12.64% | 11.38% |
| VEH gap 합계 | 58.00% | 68.29% |

### 결론

**기각.** `AH=2Ch`는 wall의 약 3.2~3.8%이고 완전 제거 상한은 **1.04배**입니다.
Task 368이 예외 축을 종결할 때와 같은 크기이므로 이 축도 종결합니다.

정적 근거가 하나 더 있습니다. 이 지연 루틴은 **자기 보정형**이라 호출당 비용이
약분됩니다. `0xDEB3B`가 1초간 호출 횟수를 세어 상수로 저장하고 `0xDEB86`이 그 상수로
나눈 횟수만큼 반복하므로, 호출을 싸게 만들면 루프가 더 돌 뿐 지연 시간은 같습니다.
즉 이 3~4%는 게임이 의도한 대기이지 제거 가능한 오버헤드가 아닙니다.

### 제 이전 서술의 오류 정정

Task 401에서 "census 표본의 95%"를 비용 근거로 쓴 것은 **범주 오류**였습니다. census
`total_cycles`는 single-step 핸들러 scope만 재고, 그 자체가 wall의 **2.04%** 입니다.
"single-step 핸들러 시간의 95%"는 "전체 비용의 95%"가 아닙니다. 세 주소를 wall 대비로
환산하면 1.30%입니다. 당시 "측정으로 확정하지 않았다"고 표시해 둔 것이 다행이었지만,
근거의 성격 자체를 잘못 읽었습니다.

`pumpit3-bring-up.md`, `current-execution-frontier.md`, census 가이드를 정정했습니다.

### 확인된 실제 비용 중심: 포트 `0x02A8` 폴링

port I/O가 wall의 약 46~56%입니다. 45초에 1,846,040회(초당 약 41,000회), 핸들러 본체만
호출당 약 29,400 cycle(약 7.4µs)로 Glide gate 호출(78,000 cycle)의 1/3입니다.

원인은 `port_io_emulator.cpp`의 `ReadJammaPort8`이 포트 1바이트마다 `GetAsyncKeyState`를
최대 5회 호출하는 것입니다. `0x02A8`의 16비트 읽기는 2바이트이므로 `in ax,dx` 한 번에
최대 10회이고, 초당 약 410,000회 커널 왕복이 됩니다.

게스트는 이 200회 읽기를 **값이 아니라 시간을 쓰려고** 실행합니다(매 반복 `eax`를 0으로
지워 결과를 버림). 실기에서는 ISA 버스 사이클 한 번짜리 싼 지연입니다.

`AH=2Ch` 지연과 결정적으로 다른 점은 **반복 횟수가 200으로 고정**이라 자기 보정되지
않는다는 것입니다. 따라서 호출당 비용을 줄이면 wall time이 실제로 줄어듭니다.

### 다음 대상 (별도 Task)

`ReadJammaPort8`이 포트 읽기마다 키보드를 전수 조회하지 않도록, 입력 스냅샷을 프레임
또는 타이머 틱 단위로 갱신하고 포트 읽기는 스냅샷을 읽게 합니다. 정확성 조건은 Task
327이 확인한 "매 폴링마다 EIP 전진 + 재트랩으로 press/release 전이를 놓치지 않을 것"이며,
스냅샷 주기가 게스트 폴링 주기보다 촘촘하면 유지됩니다.

### 미확정

- 두 실행의 프레임 수가 1,026과 867로 18% 차이 납니다. 절대 FPS는 실행 간 편차가
  크므로, 개선 검증 시 단일 실행 비교는 피해야 합니다.
- 커널 왕복 배분은 `VEH gap "other"` 평균을 호출 수에 곱한 **추정**입니다. port I/O와
  DOS 호출의 왕복 비용이 같다고 가정했습니다.
- port I/O 호출당 29,400 cycle 중 `GetAsyncKeyState`가 차지하는 비중은 직접 계측하지
  않았습니다. 다음 Task에서 분해해야 합니다.

---

## English

### Summary

Measured the `INT 21h AH=2Ch` cost hypothesis Task 401 left open against wall clock and
**rejected** it. The measurement pointed instead at the port `0x02A8` poll. No code changed.

### Results (two 45-second `aot-dbt` runs)

| Metric | base1 | base2 |
|---|---:|---:|
| Wall cycles | 177,711,788,710 | 166,469,707,454 |
| Frames | 1,026 (22.8 FPS) | 867 (19.3 FPS) |
| `AH=2Ch` calls | 263,832 | 208,740 |
| DOS service body | 0.146% | 0.047% |
| DOS body + kernel round trip | **3.84%** | **3.19%** |
| Port I/O body | 30.53% | 24.16% |
| Port I/O + kernel round trip | **56.39%** | **46.06%** |
| Glide gate | 12.64% | 11.38% |
| VEH gap total | 58.00% | 68.29% |

### Conclusion

**Rejected.** `AH=2Ch` is about 3.2-3.8% of wall clock, capping any gain at **1.04x** — the
same magnitude that closed the exception axis in Task 368, so this axis closes too.

A static argument agrees: the routine is **self-calibrating**, so cost per call cancels.
`0xDEB3B` counts calls for one second and stores the constant, and `0xDEB86` iterates a count
derived by dividing by it, so a cheaper call only spins more times over the same wall time.
The 3-4% is time the game intends to wait, not removable overhead.

### Correcting my earlier statement

Treating "95% of census samples" as a cost claim in Task 401 was a **category error**. The
census `total_cycles` measures only the single-step handler scope, itself **2.04%** of wall
clock; "95% of single-step handler time" is not "95% of cost". Converted to wall clock the
three addresses are 1.30%. Marking it unconfirmed at the time helped, but I misread what kind
of evidence it was. `pumpit3-bring-up.md`, `current-execution-frontier.md`, and the census
guide are corrected.

### Confirmed real cost centre: the port `0x02A8` poll

Port I/O is about 46-56% of wall clock: 1,846,040 observations in 45 seconds (~41,000/s) at
roughly 29,400 cycles (~7.4 µs) of handler body per call, a third of a Glide gate call's
78,000 cycles.

The cause is `ReadJammaPort8` in `port_io_emulator.cpp` calling `GetAsyncKeyState` up to five
times per port byte; a 16-bit read at `0x02A8` covers two bytes, so one `in ax,dx` performs up
to ten, giving roughly 410,000 `GetAsyncKeyState` calls per second.

The guest issues those 200 reads **for time, not data** — it zeroes `eax` each iteration and
discards the result — which on real hardware is one cheap ISA bus cycle per read. Crucially,
unlike the `AH=2Ch` delay this loop has a **fixed 200 iterations** and is not self-calibrating,
so reducing per-call cost does reduce wall time.

### Next target (separate task)

Stop `ReadJammaPort8` scanning the keyboard on every port read: refresh an input snapshot per
frame or timer tick and have port reads consume it. The accuracy constraint is the one Task
327 established — advance EIP and re-trap every poll so press/release transitions are not
latched away — which holds while the snapshot cadence is finer than the guest's polling
cadence.

### Unresolved

- Frame counts differed 18% between runs (1,026 vs 867), so absolute FPS varies enough that
  any improvement must not be judged from a single run.
- The kernel round-trip apportionment is an **estimate**: the `VEH gap "other"` mean times the
  call count, assuming port I/O and DOS calls pay the same transition.
- How much of the 29,400 cycles per port read is `GetAsyncKeyState` was not measured directly
  and should be decomposed in the next task.

---

## 추가 측정: 타이틀별 port I/O 빈도 / Follow-up: per-title port I/O frequency

### 질문

`ReadJammaPort8` 호출 빈도가 pumpit1/pumpit2/pumpit3에서 다른가?

### 정적 비교

세 실행 파일 모두 같은 "200회 `in ax,dx` I/O 지연" 루프를 가지고 있습니다.
`cmp ebx,0xC8; jl` 패턴은 pumpit1 4곳(`0x44E03` `0x450C7` `0x45377` `0x45627`),
pumpit2 4곳, pumpit3 1곳(`0x28D24`)입니다. **구조는 pumpit3만의 것이 아니며, 오히려
정적 개수는 pumpit1/pumpit2가 더 많습니다.** 따라서 정적 개수로는 답할 수 없습니다.

### 측정 (각 45초, `aot-dbt`, `REPIU_EXECUTION_TIME_PROFILE=1`)

| 타겟 | 프레임 | port I/O 호출 | 초당 | 배수 | port-io 비중 | Glide gate 비중 |
|---|---:|---:|---:|---:|---:|---:|
| pumpit1 | 2,222 | 25,091 | 558 | 1.0x | 0.42% | 57.20% |
| pumpit2 | 1,985 | 40,162 | 892 | 1.6x | 0.38% | 35.77% |
| **pumpit3** | **1,026** | **1,846,040** | **41,023** | **73.6x** | **30.53%** | **12.64%** |

**확인됨: 빈도가 타이틀마다 크게 다릅니다.** pumpit3는 pumpit1 대비 포트 접근이
**73.6배**입니다.

**확인됨: 호출당 비용은 타이틀 특성이 아닙니다.** 핸들러 본체 호출당 비용은
pumpit1 27,615 / pumpit2 15,922 / pumpit3 29,395 cycle로 같은 자릿수입니다. 즉 pumpit3의
port-io 비중이 큰 이유는 비싼 호출이 아니라 **호출 횟수**입니다. (pumpit2가 낮은 것은
읽기/쓰기 구성비 차이로 보이며 분해하지 않았습니다.)

**확인됨: 비용 구조가 타이틀마다 다릅니다.** pumpit1/pumpit2의 지배 비용은 Glide
gate(57.20% / 35.77%)이고, pumpit3는 port I/O(30.53%)입니다. 프레임도 pumpit1 2,222 /
pumpit2 1,985 대 pumpit3 1,026으로 약 2배 차이입니다.

pumpit3의 초당 41,023회는 200회 지연 루프 기준 초당 약 205회 폴링이며, 게스트가
프로그램한 PIT 240Hz와 같은 자릿수입니다. 즉 pumpit3는 타이머 주기마다 이 폴링을
수행합니다.

### Tasks 398/399/401 회귀 확인 (미뤄뒀던 항목)

pumpit1과 pumpit2 모두 45초를 크래시 없이 완주하고
`minimal execution attempt timed out`으로 종료했으며, 각각 2,222 / 1,985 프레임을
그렸습니다. pumpit1의 DOS AH hotspots는 `[3B:336 4A:261 44:97 00:92]`로 `2C`가 없어,
"pumpit1은 `AH=2Ch`를 호출하지 않는다"는 정적 분석과 일치합니다.
**세 타이틀 공유 경로 변경에 대한 회귀는 관찰되지 않았습니다.**

### 미확정

- 세 실행이 **같은 장면이 아닙니다.** 각 타이틀의 부팅 후 45초일 뿐이며, Tasks 363~368이
  확인했듯 장면이 다르면 같은 비용 집합도 몇 배씩 달라집니다. 이 표는 "타이틀별 부팅
  직후 45초"의 비교이지 동일 장면 비교가 아닙니다.
- pumpit1의 200회 루프 4곳이 런타임에 실제로 실행되는지, 아니면 pumpit1의 낮은 빈도가
  그 루프를 타지 않기 때문인지 분해하지 않았습니다.

---

## Follow-up: per-title port I/O frequency

### Question

Does `ReadJammaPort8` get called at different rates in pumpit1, pumpit2, and pumpit3?

### Static comparison

All three executables contain the same "200 x `in ax,dx`" I/O delay loop. The
`cmp ebx,0xC8; jl` pattern appears four times in pumpit1 (`0x44E03`, `0x450C7`, `0x45377`,
`0x45627`), four times in pumpit2, and once in pumpit3 (`0x28D24`). **The structure is not
unique to pumpit3 — statically pumpit1 and pumpit2 have more of them** — so static counts
cannot answer the question.

### Measured (45 seconds each, `aot-dbt`, `REPIU_EXECUTION_TIME_PROFILE=1`)

| Target | Frames | Port I/O calls | Per second | Ratio | Port-io share | Glide gate share |
|---|---:|---:|---:|---:|---:|---:|
| pumpit1 | 2,222 | 25,091 | 558 | 1.0x | 0.42% | 57.20% |
| pumpit2 | 1,985 | 40,162 | 892 | 1.6x | 0.38% | 35.77% |
| **pumpit3** | **1,026** | **1,846,040** | **41,023** | **73.6x** | **30.53%** | **12.64%** |

**Confirmed: the frequency differs sharply by title.** pumpit3 touches the ports **73.6x**
as often as pumpit1.

**Confirmed: per-call cost is not a title property.** Handler body cost per call is 27,615 /
15,922 / 29,395 cycles for pumpit1 / pumpit2 / pumpit3 — the same order. pumpit3's large
port-io share comes from **call count**, not from more expensive calls. (pumpit2's lower
figure appears to be a different read/write mix and was not decomposed.)

**Confirmed: the cost structure differs by title.** pumpit1 and pumpit2 are dominated by the
Glide gate (57.20% / 35.77%) while pumpit3 is dominated by port I/O (30.53%), and frame
counts differ about two-fold (2,222 / 1,985 versus 1,026).

pumpit3's 41,023 reads per second is about 205 polls per second at 200 reads each, the same
order as the guest-programmed 240 Hz PIT rate, so pumpit3 runs this poll once per timer tick.

### Regression check for Tasks 398/399/401 (the deferred item)

pumpit1 and pumpit2 both ran the full 45 seconds without crashing, ended with
`minimal execution attempt timed out`, and rendered 2,222 and 1,985 frames. pumpit1's DOS AH
hotspots are `[3B:336 4A:261 44:97 00:92]` with no `2C`, matching the static finding that
pumpit1 never calls `AH=2Ch`. **No regression was observed on the shared paths.**

### Unresolved

- The three runs are **not the same scene** — each is simply the first 45 seconds after boot,
  and Tasks 363-368 established that the same cost set can differ several-fold between
  scenes. This table compares "first 45 seconds per title", not equivalent scenes.
- Whether pumpit1's four 200-iteration loops execute at runtime at all, or whether its low
  frequency means that path is not taken, was not decomposed.
