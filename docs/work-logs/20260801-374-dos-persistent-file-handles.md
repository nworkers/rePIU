# 작업 로그: DOS 파일 핸들 지속화 / Work log: persistent DOS file handles

Task 374. 설계 [20260801-374](../design/20260801-374-dos-persistent-file-handles.md),
작업 지시 [20260801-374](../work-orders/20260801-374-dos-persistent-file-handles.md)

## 한국어

### 발단

사용자가 "music select의 fps가 유난히 떨어진다"며 Glide→OpenGL 구현을 의심했습니다.
측정 결과 **Glide는 병목이 아니었고**(gate 10.07%, 실제 GL work 2.80%),
`REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1`이 단일 주소를 지목했습니다.

```
cycle hotspot #1  address=0x030F87B7  count=77  cycles=1,976,177,812  max=308,199,787
                  stage: prologue 86,457 / hle 1,974,030,543 / aot-resume 2,036,184
```

호출당 **25.7M cycle(7.0 ms)**, 최악 **83.5 ms**, `hle` 단계가 99.9%.

### 원인

증거 사슬로 특정했습니다.

| 단계 | 근거 |
|---|---|
| 명령 | opcode `0xCD` → `HandleTracedDosInterrupt21` |
| 서비스 | I/O 트레이스 `op=read eip=0x030F87B7 path=...\DATAS\MODEL\T14.3DM` |
| 교차확인 | DOS 버킷 2.216e9 중 이 주소가 1.974e9 (**89%**) |

`ReadDosFile`이 읽기 **한 번마다** stat + open + seek + read + close를 했습니다.
핸들 구조체가 "경로 + 오프셋"만 들고 있어 지속 스트림이 없었기 때문입니다. Windows
`CreateFile`은 필터 드라이버 스택(실시간 검사 포함)을 통과하므로, 3.5 KB 파일의
4 KB 읽기가 6~83 ms가 됩니다. **원본 DOS는 핸들을 열어둔 채 읽습니다.**

### 구현

* `DosHostFileCache` 추가 — `std::unique_ptr<std::ifstream>` 보유.
  **복사 시 차갑게 시작**합니다. 두 핸들이 한 스트림을 공유하면 파일 위치를
  공유하게 되고, `execution_trampoline.cpp`가 상태를 복사 대입하므로 복사 가능성은
  필수입니다.
* `DosOpenFileHandle`에 `host_stream`과 `cached_file_size` 추가. 크기는 열 때 1회만
  조회하며, 이 API에 쓰기 경로가 없으므로 안전합니다.
* `ReadDosFile` — stat 제거, 캐시된 스트림 재사용. **반환값 의미는 불변**(EOF, 짧은
  읽기, 오류 0x0002/0x0005).
* `SeekDosFile` — SEEK_END에서 캐시된 크기 사용.
* `CloseDosFile` — 캐시 폐기. 닫힌 슬롯이 재사용되므로 남겨두면 이전 파일을 읽게
  됩니다.
* EOF로 failbit가 선 스트림을 다음 seek 전에 `clear()`합니다. 빠뜨리면 한 번
  EOF에 닿은 핸들의 이후 모든 접근이 조용히 실패합니다.
* 관측 추가: `file_read_count` / `host_file_open_count` + 요약 한 줄.

### 검증

* Debug/Release 빌드 성공, `repiu_aot_probe` 양 구성 **exit 0**, 신규 probe **7개 항목
  전부 true**(다중 읽기에 열기 1회, 내용·오프셋 전진, 짧은 읽기, 복사본이 차갑게
  시작하며 위치 비공유, close 시 폐기, 없는 파일 오류 코드 불변).
* 실구동(자동 장면 45초, interval 0):

```
Win32 DOS file reads/host opens/reads per open: 76/34/2.24
```

**변경 전에는 이 비가 정의상 1.00**(읽기마다 열기)이었습니다. 파일당 약 2회 읽는
접근 패턴이므로 2.24는 **파일당 열기 1회**에 해당합니다.

### music select 검증 결과 — **확정**

같은 구성(interval 0, hotspot 프로파일러 on)으로 재캡처했습니다.

| 지표 | 수정 전 | 수정 후 | 변화 |
|---|---:|---:|---:|
| **호출 수** | **77** | **77** | 동일 |
| 총 cycles | 1,976,177,812 | **76,466,549** | **−96.1%** |
| 호출당 | 25,664,647 (7.0 ms) | **993,072 (0.27 ms)** | −96.1% |
| 단일 최대 | 308,199,787 (83.5 ms) | **47,635,594 (12.9 ms)** | −84.5% |
| wall 비중 | 1.711% | **0.069%** | −1.64%p |

**호출 수가 정확히 같아 통제된 비교입니다.** 귀속도 맞습니다 — DOS 버킷이
2,215,754,976(1.92%) → 323,336,655(**0.29%**)로 **−1.89e9**인데 이 주소의 감소분
**−1.90e9**와 일치합니다. 다른 곳으로 이동한 것이 아니라 사라졌습니다. single-step
`hle` 단계 감소(−2.07e9)의 92%도 이 주소입니다.
`reads/host opens = 76/34 = 2.24`로 파일당 열기 1회입니다.

**fps는 이 수정의 성과로 주장하지 않습니다.** 프레임이 980 → 1,314(+34%),
프레임당 wall이 31.9 → 23.0 ms(−28%)로 나왔지만 이 수정이 제거한 것은 wall의
1.64%p뿐이라 28%를 설명할 수 없습니다. 프레임당 예외가 759.7 → 621.0으로 18%
줄어든 것으로 보아 장면 구성이 달랐습니다. 확정 가능한 것은 통제된 지점의
−96.1%와 그와 일치하는 DOS 버킷 감소, 그리고 **83.5 ms 스톨이 12.9 ms로 내려간
것**입니다.

### (이전 계획) 남은 검증 — music select 재캡처

자동 장면은 DOS가 원래 0.14~0.19%라 효과를 판정할 수 없습니다. **판정은 music
select에서만 가능합니다**(그 화면에서 DOS 1.92%, 문제 주소가 wall의 1.71%).

```
cmd /c "set REPIU_EXECUTION_BACKEND=aot-dbt&& set REPIU_EXECUTION_TIMEOUT_MS=0&& set REPIU_GLIDE_SWAP_INTERVAL=0&& set REPIU_EXECUTION_TIME_PROFILE=1&& set REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1&& build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit1 > musicselect3.log 2>&1"
```

판정 지표는 셋입니다.

| 지표 | 변경 전 | 기대 |
|---|---:|---|
| `0x030F87B7` 호출당 | 25.7M cycle | 크게 하락 |
| 최대 스톨 | 308M cycle (83.5 ms) | 소멸 |
| DOS wall 비중 | 1.92% | 0.1% 미만 |
| reads per open | 1.00 | 2 이상 |

### 정직한 범위

**이것은 music select 평균 fps의 주범이 아닙니다.** 그 화면이 느린 주된 이유는
게스트 계산량이 프레임당 29% 많고(경계 사이 실행 122,850 → 165,955 cycle) 예외
기구가 약 41%이기 때문입니다. 이 수정이 없애는 것은 **약 1.9%의 wall과 13프레임마다
오는 다중 프레임 끊김**이며, 끊김 쪽이 체감에서 더 큽니다. I/O가 몰리는 로딩
전환에서는 비중이 훨씬 커집니다.

---

## English

### Cause

The user suspected the Glide-to-OpenGL layer for the music select frame rate. It was
not: the Glide gate is 10.07% of wall there and actual GL work only 2.80%. The
single-step hotspot profiler named one address instead — `0x030F87B7`, 77 hits for
1,976,177,812 cycles, **25.7M per call (7.0 ms)** with a worst case of **83.5 ms**,
99.9% of it in the `hle` stage. The chain identifies it exactly: opcode `0xCD` routes
to `HandleTracedDosInterrupt21`, the I/O trace shows `op=read eip=0x030F87B7
path=...\DATAS\MODEL\T14.3DM`, and this address alone holds 89% of the DOS bucket.

`ReadDosFile` reopened the host file on every read — a stat, an ifstream
construction, a seek, the read, and a close — because the handle struct carried a
path and an offset but no stream. On Windows `CreateFile` traverses the filter driver
stack including real-time scanning, which turns a 4 KB read of a 3.5 KB file into
milliseconds. Real DOS keeps the handle open.

### Implementation

`DosHostFileCache` holds the stream and starts cold when copied, since two handles
sharing one stream would share a file position and the state is copy-assigned into
the guest thread context. The handle gained that cache and a `cached_file_size` read
once at open, which is safe while this API has no write path. Reads reuse the stream
and seeks use the cached size; close discards the cache, because closed slots are
reused and a stale stream would read the previous file. A stream that hit EOF has its
failbit cleared before the next seek — without that, every later access on a handle
that once reached EOF would silently fail. Return semantics are unchanged: EOF, short
reads, and the 0x0002 and 0x0005 error codes all behave as before.

### Verification

Both configurations build, the probe exits zero in both with all seven assertions
true, and a live 45-second run reports `DOS file reads/host opens/reads per open:
76/34/2.24`. That ratio was 1.00 by construction before the change; at roughly two
reads per file, 2.24 means one open per file.

The deciding measurement is still outstanding: the automated scene spends only
0.14–0.19% of wall in DOS, so it cannot size the fix. Music select can, where DOS was
1.92% and the offending address 1.71% of wall.

### Honest scope

This is not the main cause of the music select frame rate. That screen runs 29% more
guest computation per frame — 122,850 to 165,955 cycles between boundaries — against
exception machinery of roughly 41%. What this removes is about 1.9% of wall and a
multi-frame hitch every thirteen frames, and the hitch matters more perceptually than
the average. Wherever I/O concentrates, such as loading transitions, the share is far
larger.
