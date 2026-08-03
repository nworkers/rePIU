# 20260803-409 arena 진입 직전 예외 히스토그램 작업 지시 / Work Order

설계: [20260803-409](../design/20260803-409-arena-entry-predecessor-histogram.md)

## 한국어

### 목적

Task 408이 첫 표본 1건으로 모집단을 판정한 오류를 고친다. 주소별 진입 전이의 **직전
예외 분포**를 확보한다. 동작은 바꾸지 않는다.

### 범위

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/thread_context.h` | census 항목에 분류 계수기 4개 |
| `src/platform/win32/io/port_io_emulator.cpp` | `ApplyPortIoEntrySample`에 `switch` |
| `include/repiu/platform/win32/execution_trampoline.h` | 스냅샷 필드 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 복사 |
| `src/host/win32/main.cpp` | 주소별 세 번째 줄 |
| Task 408 작업 로그·`docs/analysis/pumpit3-bring-up.md` | 과했던 결론 정정 |

### 단계

1. `PortIoAddressCensusEntry`에 `entry_prev_single_step`, `entry_prev_breakpoint`,
   `entry_prev_access_violation`, `entry_prev_other`를 추가한다.
2. `ApplyPortIoEntrySample`에서 `sample.previous_code`를 네 갈래로 센다. 첫 표본
   보관 로직은 그대로 둔다.
3. 스냅샷에 복사하고 로그를 추가한다.
   `Win32 port I/O address #N entry prev step/bp/av/other: ...`
4. **Task 408의 결론을 정정한다.** 작업 로그 앞에 정정 절을 넣고, 분석 문서의 해당
   절을 "첫 1건의 사실이며 모집단 대표성 없음"으로 고친다. 근거 산술을 함께 남긴다.
5. Debug와 Release로 `repiu_loader_win32`, `repiu_aot_probe`를 빌드한다.

### 검증

* `repiu_aot_probe` 종료 코드 0.
* pumpit3 45초 3회 이상, 격리 없는 실행 포함. 각 주소의 분포를 읽는다.
* Task 405/406/407/408 값 불변(`cache` 0, `reentry` 0, 최다 주소 동일).
* pumpit1 45초 1회 회귀.

### 완료 조건

주소별 직전 예외 분포를 확보하고, `0x0301DB22`에 대해 **판정 가능/불가능 중 어느
쪽인지**를 근거와 함께 작업 로그에 남긴다. 불가능하면 그 이유를 명시한다.

---

## English

### Purpose

Repair Task 408's error of reading a population from one first sample by measuring the
**distribution of predecessor exceptions** per address. No behaviour change.

### Steps

1. Add `entry_prev_single_step`, `entry_prev_breakpoint`, `entry_prev_access_violation`, and
   `entry_prev_other` to `PortIoAddressCensusEntry`.
2. Classify `sample.previous_code` four ways in `ApplyPortIoEntrySample`, leaving the
   first-sample retention untouched.
3. Mirror into the snapshot and print
   `Win32 port I/O address #N entry prev step/bp/av/other`.
4. **Correct Task 408's conclusion**: add a correction section at the head of its work log and
   restate the analysis section as a fact about the first entry only, carrying the arithmetic
   that shows why.
5. Build `repiu_loader_win32` and `repiu_aot_probe` in Debug and Release.

### Verification

The probe exits zero; at least three 45-second pumpit3 runs including quarantine-free ones,
with each address's distribution read; Task 405/406/407/408 values unchanged; and one pumpit1
regression run.

### Done when

The per-address predecessor distribution is measured and the work log states, with evidence,
whether `0x0301DB22` **can or cannot** be decided — and if it cannot, why.
