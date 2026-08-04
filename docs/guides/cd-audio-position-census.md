# CD 오디오 재생 위치 census 가이드 / Measuring the CD Audio Position

노트·BGA가 음악과 어긋나거나 점프할 때, **게임이 읽는 재생 위치**를 시간축으로 찍어
원인을 가르는 절차입니다. 근거와 후보 목록은
[Task 421 설계](../design/20260805-421-cd-audio-position-reporting.md)에 있습니다.

## 1. 왜 이 census인가

게임은 **MSCDEX IOCTL 12(Q-channel)** 로 위치를 폴링하고, 그 값은
`CdAudioWaveOut::current_lba()`입니다. 종료 시 한 줄로 남던 값만으로는 **얼었는지,
뒤로 갔는지, 속도가 틀렸는지** 구분되지 않습니다. 이 census가 그 셋을 가릅니다.

**표본은 오디오 worker가 아니라 poll 스레드에서 뜹니다.** 굶은 worker는 자기
굶주림을 보고할 수 없기 때문이며, 그래서 표본마다 **직전 표본 이후 worker 반복 횟수**를
함께 적습니다.

## 2. 실행

```
set REPIU_CD_AUDIO_POSITION_CENSUS=1
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EEPROM_PATH=<실행별 사본>
build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit2 2> run.txt
```

* **`REPIU_EXECUTION_TIMEOUT_MS=0`을 쓰십시오(Tasks 421~423).** 그 값은 `INFINITE`로
  해석되어 **1초 무진행 감시를 끕니다.** 감시가 켜져 있으면 게임의 정상적인 타이머 틱
  대기 중에 실행이 종료되어 gameplay에 도달하지 못합니다(frontier 항목 1″).
  실행 시간은 하니스 쪽에서 제한하십시오.
* 표본 간격은 기본 100 ms이고 `REPIU_CD_AUDIO_POSITION_CENSUS_MS`로 바꿉니다.
  링 용량은 4,096개이므로 100 ms면 **약 6분 50초**를 담습니다.
* 결과는 `build/cd_audio_position_census.txt`이며 경로는
  `REPIU_CD_AUDIO_POSITION_CENSUS_DUMP`로 바꿀 수 있습니다.
* 자동 실행으로도 gameplay까지 갑니다 — 감시만 꺼져 있으면 됩니다.

## 3. 읽을 것

파일 헤더에 요약이 있고, 로그에는 한 줄이 나옵니다.

```
Win32 CD audio position census entries/regressions: <기록수>/<역행수>
```

각 행은 다음과 같습니다.

```
wall_ms current_lba queued_lba stream_bytes start_lba end_lba worker_iterations underruns generation playing paused delta_lba
```

* `current_lba` — **게임이 보는 값 그 자체**
* `delta_lba` — 직전 표본 대비 증감. 읽기의 중심입니다
* `worker_iterations` — 그 구간에서 오디오 worker가 돈 횟수
* `stream_bytes` — SDL 스트림 잔량(섹터 2,352바이트)

## 4. 판정 — 측정 전에 고정된 규칙

| 관측 | 결론 |
|---|---|
| `delta_lba`가 **음수**(= 요약의 `regressions` > 0) | **위치가 뒤로 감.** generation 경쟁(설계 후보 D) |
| `playing=1`인데 `delta_lba=0`이 100 ms 이상 이어지고 그때 `worker_iterations=0` | **worker 기아**(후보 B) |
| 평균 진행률이 **75 LBA/s**에서 5% 넘게 벗어남 | 재생 속도 자체 문제(별개 축) |
| 위 셋이 없고 오프셋만 일정 | **장치 버퍼**(후보 A) 또는 **pregap/논리 LBA**(후보 E). 크기로 구분 — A는 수십 ms, E는 pregap 크기 |
| `underruns`가 함께 증가 | 음악 자체가 끊긴 것이므로 위치만의 문제가 아님 |

**진행률 검산 (Task 421이 실측으로 정정):** CD는 초당 75프레임이지만 `GetTickCount`
분해능 때문에 기본 설정의 실제 표본 간격은 **109.5 ms**입니다. 따라서 정상 `delta_lba`는
7~8이 아니라 **8.21**(범위 7~9)입니다. gameplay 1,440개 표본에서 평균 8.21·이상치 0으로
확인됐습니다. **간격을 바꾸면 기대값도 다시 계산하십시오** — `75 × (실측 간격 초)`.

## 5. 한계

* 표본 간격보다 짧은 도약은 보이지 않습니다. 의심되면 `..._MS=20`으로 줄이되, 그만큼
  링이 빨리 찹니다(`overflow`가 헤더에 나옵니다).
* `playing=0` 구간의 `current_lba`는 `frozen_lba`이므로 진행하지 않는 것이 정상입니다.
* 이 census는 **읽기만** 하므로 동작을 바꾸지 않습니다. 다만 다른 census와 마찬가지로
  켠 실행의 wall·프레임을 성능 근거로 인용하지는 마십시오.

---

# Measuring the CD Audio Position

When notes and the BGA drift or jump against the music, this records **the position the game
actually reads** as a time series so the cause can be separated. The candidate list is in the
[Task 421 design](../design/20260805-421-cd-audio-position-reporting.md).

## Why

The guest polls **MSCDEX IOCTL 12 (Q-channel)**, which returns
`CdAudioWaveOut::current_lba()`. The single exit-time value cannot distinguish a position that
**froze**, one that **went backwards**, and one running at the **wrong rate**. Samples are
taken on the poll thread rather than in the audio worker, because a starved worker cannot
report its own starvation — which is why each sample also carries the **worker iterations since
the previous sample**.

## Running

Set `REPIU_CD_AUDIO_POSITION_CENSUS=1` alongside the usual backend and per-run EEPROM, and set
**`REPIU_EXECUTION_TIMEOUT_MS=0`**, which maps to `INFINITE` and **disables the one-second
no-progress watchdog**: with it enabled the run is killed during the game's ordinary timer-tick
wait and never reaches gameplay (frontier item 1''), so bound the run from the harness instead. The interval defaults to 100 ms
(`REPIU_CD_AUDIO_POSITION_CENSUS_MS`), the ring holds 4,096 samples — about six minutes fifty
at that rate — and the series lands in `build/cd_audio_position_census.txt`
(`REPIU_CD_AUDIO_POSITION_CENSUS_DUMP` overrides the path).

## Reading

The log prints `Win32 CD audio position census entries/regressions`, and each row is
`wall_ms current_lba queued_lba stream_bytes start_lba end_lba worker_iterations underruns
generation playing paused delta_lba`. `current_lba` is what the game sees and `delta_lba` is
where to look.

## Interpreting

A negative `delta_lba` — a nonzero `regressions` — means the position **moved backwards**
(candidate D). A run of `delta_lba` zero for 100 ms or more while `playing=1` **with
`worker_iterations` also zero** is worker starvation (candidate B). A rate more than 5% off
**75 LBA/s** is a playback-rate problem on its own axis. Absent those, a constant offset is the
device buffer (A) or the pregap and logical-LBA mapping (E), told apart by size. Rising
`underruns` means the music itself gapped, so the fault is not only in the position.

**Rate check (corrected by Task 421's measurement):** a CD runs 75 frames per second, but
`GetTickCount` resolution makes the real interval **109.5 ms** at the default setting, so a
healthy `delta_lba` is **8.21**, not 7 or 8, ranging 7 to 9 — confirmed as a mean of 8.21 with
no outliers over 1,440 gameplay samples. **Recompute the expectation if you change the
interval**: it is `75 × (measured interval in seconds)`.

## Limits

Jumps shorter than the sample interval are invisible — lower `..._MS` to 20 if needed, at the
cost of filling the ring sooner (the header reports `overflow`). While `playing=0` the position
is the frozen one and is expected not to advance. The census only reads existing counters and
changes no behaviour, but as with the other censuses, do not quote a census run's wall time or
frame count as performance evidence.
