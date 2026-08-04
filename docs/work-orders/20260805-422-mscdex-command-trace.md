# Task 422 작업 지시 — MSCDEX 명령 trace

설계: [20260805-422](../design/20260805-422-mscdex-command-trace.md) ·
함께 쓰는 절차: [CD 오디오 위치 census 가이드](../guides/cd-audio-position-census.md)

## 1. 구현 (완료)

| 파일 | 내용 |
|---|---|
| `include/repiu/platform/win32/mscdex_command_trace.h` | 항목·링(8,192)·기록/덤프 API, `base_tick` |
| `src/platform/win32/telemetry/mscdex_command_trace.cpp` | 환경 변수, 기록, 덤프 |
| `dpmi_mscdex_services.cpp` | `HandleMscdexRequest` switch **뒤** 한 지점에서 기록 |
| `thread_context.h` · `execution_trampoline.cpp` | 링 소유·생성, `base_tick` 설정 |
| `live_telemetry_snapshot.cpp` | teardown 덤프 |
| `execution_trampoline.h` · `main.cpp` | 요약 줄 `entries/commands` |
| `CMakeLists.txt` | 새 소스 등록 |

**동작 불변.** 스모크 확인: 명령 58건 기록, census와 **같은 시간 기준**(실행 시작 상대
ms), 요약 줄 정상.

## 2. 측정 (사용자 요청)

```
set REPIU_CD_AUDIO_POSITION_CENSUS=1
set REPIU_MSCDEX_COMMAND_TRACE=1
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=180000
set REPIU_EEPROM_PATH=<실행별 사본>
build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit2 2> run.txt
```

곡 선택 → 플레이 시작까지 진행한 뒤 **`build/mscdex_command_trace.txt`**,
**`build/cd_audio_position_census.txt`**, **`run.txt`** 를 전달합니다.
gameplay 진입에 성공한 실행이면 원래 보고된 노트·BGA 점프 구간까지 담기므로 더
가치가 큽니다.

## 3. 판정

설계 §4 표를 그대로 적용합니다. 두 파일은 시간 기준이 같으므로 census의 `playing`
전이와 trace의 명령을 **같은 축에서** 읽습니다.

## 4. 완료 기준

1. trace가 폭주의 명령 종류와 우리 응답을 특정했습니다.
2. 그에 따라 후속 수정 대상(응답 내용 / 거절 핸들러 / `Play()` 조기 반환)이 하나로
   좁혀졌습니다.
3. 작업 로그를 쓰고 frontier 항목 1′와 가이드를 갱신했습니다.

---

# Task 422 Work Order — MSCDEX command trace

Design: [20260805-422](../design/20260805-422-mscdex-command-trace.md).

## 1. Implementation (done)

A trace header and source, a record point after the dispatcher's switch in
`HandleMscdexRequest`, ownership and `base_tick` on the thread context, a teardown dump, an
`entries/commands` summary line, and the CMake registration. **Behaviour is unchanged.**
Smoke-tested: 58 commands recorded on the **same time base as the census**.

## 2. Measurement (requested from the user)

Enable both `REPIU_CD_AUDIO_POSITION_CENSUS` and `REPIU_MSCDEX_COMMAND_TRACE`, play through
song select into the start of play, and return `build/mscdex_command_trace.txt`,
`build/cd_audio_position_census.txt` and `run.txt`. A run that reaches gameplay is worth more,
since it would also capture the note and BGA jumping originally reported.

## 3-4. Verdict and done

Apply the design's table; the shared time base lets the census's `playing` transitions be read
against the trace's commands on one axis. Done when the trace names the storm's commands and
our answers, narrows the fix to one of the three candidates, and the work log, frontier item 1'
and the guide carry the result.
