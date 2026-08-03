# 20260803-406 Port I/O arena 실행 원인 작업 지시 / Work Order

설계: [20260803-406](../design/20260803-406-port-io-arena-execution-cause.md)

## 한국어

### 목적

`0x0301DB22`가 arena에서 실행되는 이유가 **번역 부재(C)** 인지 **복귀 안 함(D)** 인지
가른다. 동작은 바꾸지 않는다.

### 범위

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/thread_context.h` | census 항목에 필드 2개 |
| `src/platform/win32/io/port_io_emulator.cpp` | 스위치 판독과 두 필드 기록 |
| `include/repiu/platform/win32/execution_trampoline.h` | 스냅샷 필드 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 복사 |
| `src/host/win32/main.cpp` | 로그에 두 값 추가 |

### 단계

1. `PortIoAddressCensusEntry`에 `mapped_count`, `reentry_pending_count`를 추가한다.
2. `PortIoCensusMappingEnabled()`를 `REPIU_PORT_IO_CENSUS_MAPPING`으로 1회 캐시해
   읽는다. 기본 OFF. `1|on|true`만 ON.
3. `RecordPortIoAddress`에 두 인자를 넘긴다. `mapped`는 스위치 ON일 때만
   `FindAotCacheAddress`로 계산하고, OFF면 항상 false를 넘겨 **호출 자체를 하지 않는다.**
4. 스냅샷과 로그를 갱신한다.
   `Win32 port I/O address #N guest/count/cache/arena/mapped/reentry: ...`
5. Debug와 Release로 `repiu_loader_win32`, `repiu_aot_probe`를 빌드한다.

### 검증

* `repiu_aot_probe` 종료 코드 0.
* **스위치 OFF** pumpit3 1회: `mapped`/`reentry`가 0이고 `count`/`cache`가 Task 405와
  같은 구조(최다 주소 `0x0301DB22`, `cache` 0)인지 확인 → 동작 불변.
* **스위치 ON** pumpit3 1회: 두 필드를 읽고 설계 §판정 기준으로 결론을 낸다.
  이 실행의 wall/프레임은 인용하지 않는다.
* pumpit1 45초 1회 회귀 확인(스위치 OFF).

### 완료 조건

가설 C와 D 중 하나를 실측으로 지목하고 작업 로그에 남긴다.

---

## English

### Purpose

Decide whether `0x0301DB22` runs in the arena because **no translation exists (C)** or because
**execution never returns to the cache (D)**. No behaviour change.

### Steps

1. Add `mapped_count` and `reentry_pending_count` to `PortIoAddressCensusEntry`.
2. Read `REPIU_PORT_IO_CENSUS_MAPPING` once and cache it; default off, `1|on|true` enables.
3. Pass both to `RecordPortIoAddress`. Compute `mapped` with `FindAotCacheAddress` **only when
   the switch is on**, so the call is not made at all when off.
4. Update the snapshot and print
   `Win32 port I/O address #N guest/count/cache/arena/mapped/reentry`.
5. Build `repiu_loader_win32` and `repiu_aot_probe` in Debug and Release.

### Verification

The probe exits zero. With the switch **off**, one pumpit3 run must show `mapped` and
`reentry` at zero and the same structure Task 405 measured (top address `0x0301DB22`, `cache`
zero), proving no behaviour change. With it **on**, one pumpit3 run yields the two fields and
the design's decision rule names the cause; that run's wall time and frame count are not
quotable. One pumpit1 run with the switch off checks for regression.

### Done when

The work log names hypothesis C or D from measurement.
