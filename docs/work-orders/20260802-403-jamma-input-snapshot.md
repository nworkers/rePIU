# 20260802-403 JAMMA 입력 스냅샷 계획 / JAMMA Input Snapshot Plan

## 한국어

### 목표

Task 402가 지목한 pumpit3의 지배 비용(port I/O)의 근인을 분해하고, 정확도를 유지하면서
`GetAsyncKeyState` 호출을 줄인다.

근거는 [설계 문서](../design/20260802-403-jamma-input-snapshot.md)에 있다.

### 작업 범위

1. **분해 계측** (선행):
   `include/repiu/platform/win32/execution_trampoline.h`의 `Win32PortIoObservation`에
   `jamma_scan_cycles` / `jamma_scan_count` / `key_query_count` 추가,
   `port_io_emulator.cpp`에서 rdtsc 구간과 호출 계수, `main.cpp`에 보고 한 줄.
2. **스냅샷 구현**:
   `ReadJammaPort8`을 `ScanJammaPort8`(실제 조회)과 스냅샷 조회로 분리.
   `REPIU_JAMMA_SNAPSHOT`(기본 on, `0`이면 off), `REPIU_JAMMA_SNAPSHOT_US`(기본 500).
3. 문서: 설계, 작업 로그, `docs/analysis/piu-io-port-specification.md`,
   `docs/analysis/current-execution-frontier.md`.

### 검증 절차

**실행 간 편차가 크므로 각 3회 이상, 중앙값으로 판정한다.** 그리고 `eeprom.dat`는
추적되지 않는 영속 상태이므로 **매 실행마다 고정 fixture를 복사해
`REPIU_EEPROM_PATH`로 격리한다**(기존 `scripts/benchmark_*.ps1`이 쓰는 방식).

```
copy build\repiu-task383-baseline-eeprom.dat %SCRATCH%\ee_run.dat
set REPIU_EEPROM_PATH=%SCRATCH%\ee_run.dat
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=45000
set REPIU_EXECUTION_TIME_PROFILE=1
set REPIU_JAMMA_SNAPSHOT=0   (대조) / 1 (실험)
build\Release\repiu_loader_win32.exe pumpit3 > repiu_ab.log 2>&1
```

비교 지표: `Win32 JAMMA scan cycles/scans/key-queries`, port-io wall 비중,
`_GRBUFFERSWAP@4` 프레임 수. 마지막으로 pumpit1/pumpit2 회귀 확인.

---

## English

### Objective

Decompose the root cause of pumpit3's dominant cost (port I/O, identified in Task 402) and
reduce `GetAsyncKeyState` calls without losing accuracy. Rationale is in the
[design document](../design/20260802-403-jamma-input-snapshot.md).

### Task Scope

1. **Decomposition instrumentation** first: add `jamma_scan_cycles`, `jamma_scan_count`, and
   `key_query_count` to `Win32PortIoObservation`, time the scan loop with rdtsc, count the
   calls, and report one line from `main.cpp`.
2. **Snapshot**: split `ReadJammaPort8` into `ScanJammaPort8` (the real query) and a snapshot
   lookup, with `REPIU_JAMMA_SNAPSHOT` (default on, `0` disables) and
   `REPIU_JAMMA_SNAPSHOT_US` (default 500).
3. Documentation: design, work log, `docs/analysis/piu-io-port-specification.md`, and
   `docs/analysis/current-execution-frontier.md`.

### Verification Procedure

**Run-to-run variance is large, so use at least three runs per arm and judge on the median.**
`eeprom.dat` is untracked persistent state, so **copy a fixed fixture per run and isolate it
with `REPIU_EEPROM_PATH`**, the method the existing `scripts/benchmark_*.ps1` already use.

Compare `Win32 JAMMA scan cycles/scans/key-queries`, the port-io wall share, and
`_GRBUFFERSWAP@4` frames between `REPIU_JAMMA_SNAPSHOT=0` and the default, then check pumpit1
and pumpit2 for regressions.
