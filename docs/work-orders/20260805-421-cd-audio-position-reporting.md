# Task 421 작업 지시 — CD 오디오 재생 위치

설계: [20260805-421](../design/20260805-421-cd-audio-position-reporting.md) ·
측정 절차: [CD 오디오 위치 census 가이드](../guides/cd-audio-position-census.md)

## 1. 1단계 — 스핀 배제 (완료)

`REPIU_GLIDE_RENDEZVOUS_SPIN_US=0`으로도 **증상 동일**(사용자 확인, 2026-08-05).
설계 §5의 후보 B(Task 419 스핀에 의한 worker 기아)는 **그 원인으로는 배제**됩니다.
worker 기아 자체는 다른 원인으로 여전히 가능하므로 census가 계속 확인합니다.

## 2. 2단계 — 계측 (완료)

| 파일 | 내용 |
|---|---|
| `include/repiu/platform/win32/cd_audio_position_census.h` | 표본 구조·링(4,096)·기록/덤프 API |
| `src/platform/win32/telemetry/cd_audio_position_census.cpp` | 환경 변수 해석, 역행 카운트, 덤프 |
| `cd_audio_wave_out.h/.cpp` | `FillPositionSample`, `worker_iterations`·`underruns` 카운터 |
| `thread_context.h` · `execution_trampoline.cpp` | census 소유·생성 |
| `live_telemetry_snapshot.cpp` | **poll 스레드**에서 표본, teardown에서 덤프 |
| `execution_trampoline.h` · `main.cpp` | 요약 줄 `entries/regressions` |
| `CMakeLists.txt` | 새 소스 등록 |

**동작 불변** — 이미 있는 값을 읽기만 합니다. 스모크 확인: 표본 10건 기록, worker가
100 ms당 18~44회 반복, 덤프·요약 줄 정상.

## 3. 3단계 — 사용자 측정 (요청)

가이드대로 **증상이 보이는 gameplay까지 플레이**한 뒤 다음 두 가지를 전달해 주십시오.

1. `build/cd_audio_position_census.txt`
2. 실행 로그(stderr)

자동 실행은 attract 데모에서 멈추므로(frontier 항목 1′) 이 구간은 사용자만 잡을 수
있습니다.

## 4. 4단계 — 판정과 수정

설계 §4의 표를 그대로 적용해 A~E 중 하나를 지목한 뒤 수정합니다. **판정 전에는
고치지 않습니다**(설계 §6). 단 후보 D(generation 경쟁,
`cd_audio_wave_out.cpp:173`)는 판정과 무관하게 정확성 결함이므로 그 다음에 고칩니다.

## 5. 완료 기준

1. census 시계열이 A~E 중 하나를 지목했습니다.
2. 수정 후 같은 census로 **증상 구간이 사라졌음**을 보였습니다.
3. 작업 로그를 쓰고 frontier와 가이드를 결과로 갱신했습니다.

---

# Task 421 Work Order — CD audio position

## 1. Step one — the spin is excluded (done)

`REPIU_GLIDE_RENDEZVOUS_SPIN_US=0` leaves the symptom unchanged (user, 2026-08-05), so
candidate B **as caused by Task 419's spin** is out. Worker starvation from another source
remains possible and the census still checks for it.

## 2. Step two — instrumentation (done)

A new census header and source, `FillPositionSample` plus `worker_iterations` and `underruns`
counters on the audio backend, ownership on the thread context, sampling on the **poll thread**
with the dump at teardown, an `entries/regressions` summary line, and the CMake registration.
**Behaviour is unchanged** — it only reads values the audio path already keeps. Smoke-tested:
ten samples recorded, the worker iterating 18-44 times per 100 ms, dump and summary both
produced.

## 3. Step three — the user's measurement (requested)

Follow the guide, **play far enough to see the symptom**, and send back
`build/cd_audio_position_census.txt` together with the run's stderr log. Automated runs stall
in the attract demo (frontier item 1'), so only a real play session reaches this.

## 4-5. Verdict, fix, and done

Apply the design's table unchanged to name one of A through E, then fix it — **no fixing before
the verdict** (design section 6), except that candidate D, the generation race at
`cd_audio_wave_out.cpp:173`, is a correctness defect and is repaired afterwards regardless.
Done when the series names a cause, the same census shows the symptom interval gone after the
fix, and the work log, frontier and guide carry the result.
