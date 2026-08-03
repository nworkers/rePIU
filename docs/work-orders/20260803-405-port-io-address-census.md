# 20260803-405 Port I/O 주소 census 작업 지시 / Port I/O Address Census Work Order

설계: [20260803-405](../design/20260803-405-port-io-address-census.md)

## 한국어

### 목적

port I/O의 98.6%가 예외를 냅니다. 그 명령들이 **AOT 캐시 안에서 실행되는지 arena에서
네이티브로 실행되는지**, 그리고 **어느 주소인지**를 확보합니다. 동작은 바꾸지 않습니다.

### 범위

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/thread_context.h` | census 구조체와 카운터 |
| `src/platform/win32/io/port_io_emulator.cpp` | `decode_eip` 확정 직후 기록 |
| `include/repiu/platform/win32/execution_trampoline.h` | 스냅샷 필드 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 스냅샷 복사 |
| `src/host/win32/main.cpp` | 요약 로그 2종 |

### 단계

1. `kPortIoAddressCensusCapacity = 32`, `PortIoAddressCensusEntry`
   (`guest_address`, `count`, `cache_count`), 배열, `..._size`, `..._overflow`를 추가한다.
2. `HandlePortIoInstruction`에서 `decode_eip`와 캐시 판정이 끝난 직후 기록한다.
   조기 반환(`IsGuestRangeReadable` 실패, opcode 불일치)보다 **뒤**에 두어 census가
   실제로 처리된 port I/O만 세게 한다.
3. 스냅샷에 같은 필드를 추가하고 복사한다.
4. 요약 로그를 추가한다.
   * `Win32 port I/O address census entries/overflow/total: {}/{}/{}`
   * `Win32 port I/O address #{} guest/count/cache/arena: ...` (`count` 내림차순, 최대 16)
5. Debug와 Release로 `repiu_loader_win32`, `repiu_aot_probe`를 빌드한다.

### 검증

* `repiu_aot_probe` 두 구성 종료 코드 0, 기존 항목 전부 true.
* pumpit3 45초 실행에서 **census 합계 + overflow == `execution time count`의 port-io 값**.
  불일치는 기록 누락이므로 실패로 본다.
* pumpit1 45초 1회 회귀 확인.

### 완료 조건

최다 주소와 `cache`/`arena` 배분을 실측으로 확보하고, 설계 §판정 기준에 따라 다음 축을
지목해 작업 로그에 남긴다.

---

## English

### Purpose

98.6% of port I/O takes an exception. Establish whether those instructions execute **inside
the AOT cache or natively in the arena**, and **at which addresses**. No behaviour change.

### Scope

`thread_context.h` gains the census; `port_io_emulator.cpp` records one entry once
`decode_eip` and the cache decision are resolved; the snapshot header and copier forward the
fields; `main.cpp` prints two summary lines.

### Steps

1. Add `kPortIoAddressCensusCapacity = 32`, `PortIoAddressCensusEntry` with
   `guest_address`, `count`, and `cache_count`, plus size and overflow counters.
2. Record in `HandlePortIoInstruction` immediately after `decode_eip` and the cache decision,
   placed **after** the early returns for unreadable ranges and non-port opcodes so the
   census counts only port I/O actually handled.
3. Mirror and copy the fields in the snapshot.
4. Print `Win32 port I/O address census entries/overflow/total` and up to sixteen
   `Win32 port I/O address #N guest/count/cache/arena` lines, sorted by count.
5. Build `repiu_loader_win32` and `repiu_aot_probe` in Debug and Release.

### Verification

Both probe configurations exit zero with existing checks true; a pumpit3 run where the census
total plus overflow equals the profiled port-io count, treating any mismatch as a failure;
and one pumpit1 run for regression.

### Done when

The top address and the cache/arena split are measured, and the work log names the next axis
using the design's decision rule.
