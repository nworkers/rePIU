# 20260803-410 VEH 종료 지점 귀속 작업 지시 / VEH Exit Site Attribution Work Order

설계: [20260803-410](../design/20260803-410-veh-exit-site-attribution.md)

## 한국어

### 범위

VEH가 예외를 재개시키는 지점에 이름을 붙이고, 그 이름을 다음 예외가 읽을 수 있게
전이 슬롯과 히스토그램에 남깁니다. **동작 변경 없음.**

### 변경 대상

| 파일 | 변경 |
|---|---|
| `include/repiu/platform/win32/veh_exit_site.h` (신규) | `VehExitSite` 열거형, `kVehExitSiteCount`, `VehExitSiteName` |
| `include/repiu/platform/win32/execution_trampoline.h` | `Win32ExecutionAttempt` 스냅샷에 히스토그램·진입 표본 필드 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 스냅샷 복사와 용량 `static_assert` |
| `src/platform/win32/execution/thread_context.h` | `last_veh_exit_*`/`prev_veh_exit_*`, arena single-step 히스토그램, `PortIoAddressCensusEntry`에 진입 표본 2필드 |
| `src/platform/win32/execution/execution_internal.h` | `NoteVehExitSite` 선언 |
| `src/platform/win32/execution/execution_trampoline.cpp` | `VehExitRecorder` 생성, 전이 슬롯 갱신, 각 종료 지점 태그 |
| `src/platform/win32/aot/aot_runtime_dispatch.cpp` | reentry·write 경로 4개 지점 태그 |
| `src/platform/win32/io/port_io_emulator.cpp` | 진입 표본에 exit site·exit eip 추가 |
| `src/host/win32/main.cpp` | 히스토그램과 확장된 진입 표본 출력 |
| `CMakeLists.txt` | 신규 헤더가 소스 목록에 필요하면 추가 |

### 절차

1. 열거형과 헤더를 먼저 만든다. 값은 단조 증가하는 `std::uint8_t`이고 `kUnknown = 0`.
2. `ThreadContext`에 슬롯을 추가한다. 기존 `last_veh_*` 주석 바로 아래에 둔다.
3. VEH에 `VehExitRecorder`를 넣는다. **`AotHleTranslationScope`보다 먼저 생성**할 것.
   순서가 뒤집히면 EIP 재작성 전 값을 기록하게 되어 §5 판정이 무의미해진다.
4. 종료 지점 태그를 단다. single-step을 소비할 수 있는 경로를 우선한다.
5. arena EIP single-step 히스토그램을 `RecordVehExceptionCensus` 근처가 아니라
   **소멸자**에서 센다. 소비자가 정해진 뒤여야 하기 때문이다.
6. port I/O 진입 표본과 출력을 확장한다.
7. Release 빌드와 `repiu_aot_probe`를 돌린다.

### 검증

* Release 빌드 성공, `repiu_aot_probe` 종료 코드 0.
* **검산:** `veh_arena_single_step_count`가 종료 지점 카운터 합과 같을 것.
* **회귀 없음 확인:** 같은 세션에서 pumpit1 45초 실행이 오늘 범위 안일 것,
  port I/O census의 `cache` 0·`reentry` 0·최다 주소가 Task 405/406/409와 같을 것.
* 동작 불변이므로 프레임 수 변화는 세션 변동폭 안이어야 한다.

### 하지 않을 것

* 복귀 경로 구현(frontier 항목 3). 시작점이 확정되기 전에는 같은 이탈이 반복된다.
* 진입 횟수 편차 조사(frontier 항목 1). 실행이 필요하므로 사용자 실행 후로 미룬다.
* `REPIU_PORT_IO_CENSUS_MAPPING`을 켠 측정. 그 실행의 wall·프레임은 인용 금지다.

---

## English

### Scope

Name every point at which the VEH resumes an exception and make that name readable by
the next exception, through the existing transition slots plus a histogram.
**No behavioural change.**

### Files

A new `veh_exit_site.h` holds the enumeration, its count, and a name function.
`thread_context.h` gains the `last_veh_exit_*`/`prev_veh_exit_*` pair, the arena
single-step histogram, and two fields on the port I/O census entry.
`execution_internal.h` declares `NoteVehExitSite`. `execution_trampoline.cpp` creates
the recorder and tags its exits; `aot_runtime_dispatch.cpp` tags the four re-entry and
write paths; `port_io_emulator.cpp` carries the predecessor's exit site and EIP into
the entry sample; `main.cpp` prints both.

### Procedure

Build the enumeration first, then the context slots, then the recorder — which **must
be constructed before `AotHleTranslationScope`**, since the reverse order would record
the EIP before that scope rewrites it and make the design's decision rules meaningless.
Tag the exits, counting the arena single-step histogram **in the destructor** rather
than at census time, because the consumer is only known by then. Extend the port I/O
sample and the report last.

### Verification

Release build passes and `repiu_aot_probe` exits zero. The **sum check** must hold:
`veh_arena_single_step_count` equals the total of the exit-site counters. Behaviour
must be unchanged — `cache` zero, `reentry` zero, and the same top address as Tasks
405, 406, and 409 — and a 45-second pumpit1 run must land inside the session's range.

### Out of scope

The arena-to-cache return (frontier item 3), which would replay the same departure
until its head is known; the entry-count variance (item 1), which needs runs; and any
measurement with `REPIU_PORT_IO_CENSUS_MAPPING` enabled, whose wall time and frames are
not quotable.
