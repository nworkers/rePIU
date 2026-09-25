# Task 719 작업 로그 — 장면별 시각을 두 host에서 비교

설계: [20260919-719](../design/20260919-719-scene-timeline.md) ·
작업 지시: [20260919-719](../work-orders/20260919-719-scene-timeline.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260919-718](20260919-718-default-safe-point-injection.md)

## 요약

**Linux x64가 뒤처지는 것은 장면 진행 논리가 아니라 로딩 구간의 정지 때문입니다.** 같은
장면이 같은 순서로 나오고 평상시 초당 swap 수도 거의 같지만, Linux에는 swap이 1–3.6초씩
멈추는 구간이 다섯 곳 있습니다(Win32는 한 곳, 0.8초). 가장 유력한 원인은 **Linux x64에
Glide gate 직접 디스패치가 없는 것**입니다. Win32는 Glide 호출 252만 번을 trap 없이
처리하는데, Linux는 180초 동안 242만 번을 전부 `ud2` trap과 signal로 처리합니다.

## 진단 추가

`REPIU_GLIDE_PIXEL_DIAG_INTERVAL_MS=<ms>`를 설정하면 swap 번호 대신 시간 기준으로 표본을
뜹니다(이 변수만으로 켜지며 `REPIU_GLIDE_PIXEL_DIAG`는 필요 없음).

```text
[repiu-scene] t_ms=N swap=K non_black=P avg=R,G,B grid=XXXXXXXXXXXXXXXX
```

`grid`는 화면을 4×4로 나눈 칸별 평균 밝기(0–F)입니다. 설정하지 않으면 출력이 바뀌지
않습니다.

## 측정

두 host를 **순서대로** 180초씩, 500ms 간격으로 기록했습니다. 처음에는 두 실행을 동시에
돌렸는데, CPU를 나눠 써서 시각 비교에 쓸 수 없어 버리고 다시 기록했습니다. 두 host 모두
폴트 0입니다.

### 공통 장면으로 맞춘 시각(초, 첫 swap 기준)

| 장면(grid 앞부분) | Win32 1주기 | Linux 1주기 | Win32 2주기 | Linux 2주기 |
|---|---:|---:|---:|---:|
| 주황 인트로 `…0266` | 4.0 | 3.0 | 81.5 | 86.5 |
| `54664469…` | 19.1 | — | 96.0 | 104.5 |
| `50227000…` | 22.5 | — | 99.0 | 108.0 |
| `AAA9AA96…` | 25.5 | 25.5 | 102.0 | 112.0 |
| `DCDB9AA9…` | 53.0 | 58.0 | — | — |
| `BBB6BB52…` | 59.5 | 63.5 | — | — |

주황 인트로 뒤 `54664469…`까지 걸린 시간은 Win32 14.5초, Linux 18.0초입니다.

### 그 사이에 일어난 일(2주기)

| 구간 | Win32 | Linux |
|---|---|---|
| 주황 인트로(`…0266` 정지) | 82.0–84.5초 | 87.0–89.5초 |
| 검은 화면 | 85.0초에 표본 1개 | 90.0–92.4초, **swap 6199→6237이 1.9초** |
| 다음 장면(`1100111…`→`5555…`) | 9.0초, 270 swap(초당 30) | 10.5초, 271 swap(초당 26) |

다음 장면은 swap 수로 진행하는 장면이라 두 host가 같은 swap 수(270)를 쓰고, Linux는
초당 swap이 적어 1.5초 더 걸립니다.

### swap이 멈춘 구간(표본 간격 700ms 초과)

| Win32 | Linux |
|---|---|
| 27.5–28.3초(0.8초) | 7.0–8.4초(1.4초) |
| | 28.0–30.5초(2.5초) |
| | 82.0–83.0초(1.0초) |
| | 90.5–92.4초(1.9초) |
| | 114.5–118.1초(3.6초) |

28초와 114.5초의 정지는 게임이 틱 카운터를 되돌리는 **주기 경계**, 즉 다음 attract
주기를 로딩하는 지점이고, Win32도 같은 자리(27.5초)에서 멈춥니다. 정지 중에도 게임 틱은
초당 약 230씩 흐르므로 게스트는 멈춘 것이 아니라 일을 하고 있습니다.

## 정지 중에 게스트가 하는 일

정지 구간의 `last_eip`는 cache `0x20127C38`에 머물렀습니다. peek로 그 바이트를 읽으면
(`REPIU_LIVE_GUEST_PEEK=0x1F127C20,16`) `popad` lowering, `CC`, `FB`(`sti`), 그리고 게스트
`0x010F2787`로 가는 `ret` lowering입니다. 게스트 `0x010F2786`은 `sti; ret` 도우미이고,
`cli; ret`와 포트 입출력 도우미가 그 옆에 있습니다. 즉 게스트는 인터럽트를 껐다 켜는
루틴을 반복하고, 그 `sti`마다 특권 명령 폴트를 거칩니다.

## 두 host의 차이를 만드는 것

| 180초 합계 | Win32 | Linux x64 |
|---|---:|---:|
| 예외 처리 진입 | 522,651 | **2,548,911** |
| AOT boundary | 123,341 | **2,510,694** |
| 그중 `0F 0B`(`ud2`, Glide gate) | — | **2,425,507** |
| Glide 직접 디스패치 | 2,522,747 | 0(`capable=false`) |
| 포트 I/O | 276,164 | 266,644 |
| DOS 파일 read | 112 | 109 |

Win32는 Glide gate의 `ud2`를 `call thunk; ret n`으로 패치해 trap 없이 호출합니다(Task
424 계열의 직접 디스패치). 그 thunk(`AotDbtGlideGateDispatchThunk`)는 i386 어셈블리뿐이라
x64에서 `GetGlideGateDirectDispatchThunkAddress()`가 `nullptr`을 돌려주고, Linux의 모든
Glide 호출이 signal을 거칩니다. 포트 I/O와 파일 read는 두 host가 비슷하므로 차이는 Glide
gate에서 옵니다.

**확인하지 않은 것:** 정지 구간 자체가 Glide 호출로 채워져 있는지는 측정하지 않았습니다.
그 구간의 `last_eip`는 `sti`였고, 로딩 중 Glide 호출(텍스처 다운로드 등)이 몰린다는 것은
추정입니다. 직접 디스패치를 만든 뒤 같은 기록으로 정지가 줄어드는지 보는 것이 판정입니다.

## 검증

* Linux x64 core probe **30/30**, Win32 x86 core probe **28/28**, Win32 전체 빌드 오류 0
* 변수를 설정하지 않은 Task 718 실행들과 출력 형식이 같음(새 줄은 변수가 있을 때만)

## 다음

**Task 720 — Linux x64 Glide gate 직접 디스패치.** x64 thunk를 만들어 long-mode cache에서
gate를 trap 없이 호출합니다. 판정: 같은 180초 기록에서 `ud2` boundary와 swap 정지가
줄어드는지.

---

## English

Design: [20260919-719](../design/20260919-719-scene-timeline.md) ·
Work order: [20260919-719](../work-orders/20260919-719-scene-timeline.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260919-718](20260919-718-default-safe-point-injection.md)

### Summary

**Linux x64 falls behind because of stalls in loading stretches, not because of the
scene logic.** The same scenes appear in the same order and the ordinary swap rate is
nearly the same, but Linux has five stretches where swaps stop for 1 to 3.6 seconds
(Win32 has one, 0.8 seconds). The most likely cause is that **Linux x64 has no
direct Glide gate dispatch**: Win32 makes 2.52 million Glide calls without a trap,
while Linux takes all 2.42 million of its calls in 180 seconds through a `ud2` trap
and a signal.

### Added diagnostic

`REPIU_GLIDE_PIXEL_DIAG_INTERVAL_MS=<ms>` samples by time instead of swap number (it
enables sampling by itself; `REPIU_GLIDE_PIXEL_DIAG` is not needed) and writes
`[repiu-scene] t_ms=N swap=K non_black=P avg=R,G,B grid=XXXXXXXXXXXXXXXX`, where
`grid` is the mean brightness (0–F) of each cell of a 4×4 split. Unset, output is
unchanged.

### Measurement

Both hosts were recorded **one after the other**, 180 seconds each at 500 ms. The
first attempt ran them at the same time; they shared the CPU, so it was discarded.
No faults on either host.

Aligned on scenes both hosts show (seconds from the first swap): the orange intro at
4.0 / 3.0 (Win32 / Linux, first cycle) and 81.5 / 86.5 (second); `54664469…` at
19.1 / — and 96.0 / 104.5; `50227000…` at 22.5 / — and 99.0 / 108.0; `AAA9AA96…` at
25.5 / 25.5 and 102.0 / 112.0; `DCDB9AA9…` at 53.0 / 58.0; `BBB6BB52…` at
59.5 / 63.5. From the orange intro to `54664469…` takes 14.5 s on Win32 and 18.0 s on
Linux.

In the second cycle: the orange intro holds for 82.0–84.5 s on Win32 and
87.0–89.5 s on Linux. The black screen after it is a single sample at 85.0 s on
Win32 but lasts 90.0–92.4 s on Linux, with **swaps 6199→6237 taking 1.9 s**. The next
scene (`1100111…` to `5555…`) takes 9.0 s and 270 swaps on Win32 (30 per second) and
10.5 s and 271 swaps on Linux (26 per second): it advances by swaps, so both spend the
same 270, and Linux, swapping less often, takes 1.5 s longer.

Swap stalls (a sample gap over 700 ms): Win32 27.5–28.3 s (0.8 s); Linux 7.0–8.4 s
(1.4), 28.0–30.5 s (2.5), 82.0–83.0 s (1.0), 90.5–92.4 s (1.9) and 114.5–118.1 s
(3.6). The stalls at 28 and 114.5 s are the **cycle boundaries** where the game winds
its tick counter back — the load of the next attract cycle — and Win32 stalls at the
same point (27.5 s). The game's ticks keep climbing about 230 per second through the
stalls, so the guest is working, not stuck.

### What the guest does in a stall

`last_eip` stayed at cache `0x20127C38`. Its bytes, read with
`REPIU_LIVE_GUEST_PEEK=0x1F127C20,16`, are a `popad` lowering, `CC`, `FB` (`sti`) and a
`ret` lowering to guest `0x010F2787`. Guest `0x010F2786` is a `sti; ret` helper, beside
`cli; ret` and the port I/O helpers: the guest repeatedly runs a routine that masks and
unmasks interrupts, and each `sti` goes through a privileged-instruction fault.

### What makes the hosts differ

Over 180 seconds: exception dispatches 522,651 on Win32 against **2,548,911** on
Linux; AOT boundaries 123,341 against **2,510,694**, of which **2,425,507** are `0F 0B`
(`ud2`, the Glide gates); direct Glide dispatches 2,522,747 against 0
(`capable=false`); port I/O 276,164 against 266,644; DOS file reads 112 against 109.

Win32 patches each Glide gate's `ud2` into `call thunk; ret n` and calls it without a
trap. That thunk (`AotDbtGlideGateDispatchThunk`) exists only as i386 assembly, so on
x64 `GetGlideGateDirectDispatchThunkAddress()` returns `nullptr` and every Linux Glide
call goes through a signal. Port I/O and file reads are alike on both hosts, so the
difference comes from the Glide gates.

**Not checked:** whether the stalls themselves are filled with Glide calls. The
`last_eip` there was the `sti`; that loading bunches Glide calls (texture downloads
and the like) is an inference. The test is whether the stalls shrink in the same
recording once direct dispatch exists.

### Verification

Linux x64 core probe **30 of 30**, Win32 x86 **28 of 28**, full Win32 build with no
errors. Without the variable, output has the same form as Task 718's runs; the new
line appears only when it is set.

### Next

**Task 720 — direct Glide gate dispatch on Linux x64**: an x64 thunk so long-mode cache
code calls the gates without a trap. The test: whether `ud2` boundaries and swap
stalls fall in the same 180-second recording.
