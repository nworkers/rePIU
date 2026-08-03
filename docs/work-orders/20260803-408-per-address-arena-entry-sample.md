# 20260803-408 주소별 arena 진입 표본 작업 지시 / Work Order

설계: [20260803-408](../design/20260803-408-per-address-arena-entry-sample.md)

## 한국어

### 목적

정상 모드에서 `0x0301DB22`가 arena로 진입하는 신호를 확보한다. 동작은 바꾸지 않는다.

### 범위

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/thread_context.h` | census 항목에 진입 표본 필드 |
| `src/platform/win32/io/port_io_emulator.cpp` | 주소별 첫 진입 기록 |
| `include/repiu/platform/win32/execution_trampoline.h` | 스냅샷 필드 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 복사 |
| `src/host/win32/main.cpp` | 주소별 로그에 진입 표본 추가 |

### 단계

1. `PortIoAddressCensusEntry`에 `entry_transition_count`, `entry_previous_code`,
   `entry_previous_eip`, `entry_flags`를 추가한다.
2. `RecordPortIoAddress`에 진입 여부와 상태를 넘긴다. 진입이면 count를 올리고,
   그 주소의 **첫 진입일 때만** code/eip/flags를 채운다.
3. `entry_flags` 비트: 0 prev-in-cache, 1 tf, 2 reentry, 3 legacy, 4 single-step.
4. 스냅샷과 로그를 갱신한다. 주소별 두 번째 줄로 낸다.
   `Win32 port I/O address #N entry count/prev-code/prev-eip/flags: ...`
5. Debug와 Release로 `repiu_loader_win32`, `repiu_aot_probe`를 빌드한다.

### 검증

* `repiu_aot_probe` 종료 코드 0.
* pumpit3 45초 3회. **격리 없는 실행이 최소 1회** 포함돼야 하며(로그의
  `AOT generation publishes/quarantines`가 `.../0`), 없으면 추가 실행한다.
* `0x0301DB22`의 `entry_transition_count`와 `count`의 비가 약 1:200인지 확인한다.
  크게 다르면 설계 §판정 기준 마지막 행에 따라 진입 정의를 재검토한다.
* Task 405/406/407 값이 그대로인지 확인.
* pumpit1 45초 1회 회귀.

### 완료 조건

정상 모드 `0x0301DB22`의 진입 신호를 실측으로 확보하고 다음 축을 지목한다.
확보하지 못하면 그 사실과 이유를 작업 로그에 남긴다.

---

## English

### Purpose

Capture the signature by which `0x0301DB22` enters arena execution in healthy runs. No
behaviour change.

### Steps

1. Add `entry_transition_count`, `entry_previous_code`, `entry_previous_eip`, and
   `entry_flags` to `PortIoAddressCensusEntry`.
2. Pass the entry condition and state into `RecordPortIoAddress`; on an entry, increment the
   count and fill code, EIP, and flags **only on that address's first entry**.
3. Flag bits: 0 prev-in-cache, 1 trap flag, 2 re-entry pending, 3 legacy fallback, 4
   single-step trace.
4. Mirror into the snapshot and print a second per-address line,
   `Win32 port I/O address #N entry count/prev-code/prev-eip/flags`.
5. Build `repiu_loader_win32` and `repiu_aot_probe` in Debug and Release.

### Verification

The probe exits zero; three 45-second pumpit3 runs including **at least one without quarantine**
(`AOT generation publishes/quarantines` reading `.../0`), adding runs if none occurs; the ratio
of `entry_transition_count` to `count` at `0x0301DB22` checked against the expected one per two
hundred, revisiting the entry definition if it differs sharply; Task 405/406/407 values
unchanged; and one pumpit1 regression run.

### Done when

The healthy-mode entry signature for `0x0301DB22` is measured and the next axis named, or the
failure to obtain it is recorded with its reason.
