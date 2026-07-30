# 20260730-367 HLE boundary opcode 귀속 작업 지시 / Work order

* 설계: [20260730-367-hle-boundary-opcode-attribution.md](../design/20260730-367-hle-boundary-opcode-attribution.md)
* 근거: [Task 366 작업 로그](../work-logs/20260730-366-timer-tick-delivery-and-frame-pacing.md)
* 범위: **계측만.** 명령 의미·HLE 경계·동작 변경 없음.

## 한국어

### 1. 구현 항목

| # | 파일 | 내용 |
|---|---|---|
| 1 | `include/repiu/platform/win32/aot_boundary_opcode_census.h` | prefix 건너뛰기, 실효 opcode·`0F` map·prefix 카운트 자료형과 API |
| 2 | `src/platform/win32/aot/aot_boundary_opcode_census.cpp` | 위 구현 |
| 3 | `src/platform/win32/execution/thread_context.h` | census 보유 |
| 4 | `src/platform/win32/aot/aot_runtime_dispatch.cpp` | `RecordAotOtherBoundarySample`에서 census 호출 |
| 5 | `include/repiu/platform/win32/execution_trampoline.h` | attempt 스냅샷 필드(상위 N + 합계) |
| 6 | `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 상위 N 추출과 정렬(종료 시에만) |
| 7 | `src/host/win32/main.cpp` | 종료 요약 |
| 8 | `src/tools/aot_probe/aot_boundary_opcode_census_probe.{h,cpp}` | 단위 probe |
| 9 | `src/tools/aot_probe/main.cpp`, `CMakeLists.txt` | probe 등록과 빌드 |
| 10 | `scripts/task367_hle_boundary_opcode_attribution.ps1` | 3회 측정과 gate N1~N5 |

### 2. 필수 제약

* **동작을 바꾸지 않습니다.** 세는 대상만 늘립니다. 기존 `bytes[0]` histogram은
  그대로 유지합니다.
* prefix 건너뛰기는 최대 4개까지만 하고 초과 시 `prefix_overflow`로 셉니다. 잘못된
  바이트열에서 무한 전진하지 않도록 길이 경계를 지킵니다.
* `0F` 뒤에 바이트가 없으면 `escape_truncated`로 셉니다.
* hot path에서 allocation·문자열·정렬·clock read를 하지 않습니다. 정렬과 출력은 종료
  시에만 합니다.
* 표본 수는 기존 histogram과 **반드시 일치**해야 합니다(gate N1).

### 3. 검증 절차

1. `scripts/build_win32_x86.bat` (Debug) 통과
2. `scripts/build_win32_x86_release.bat` (Release) 통과
3. `repiu_aot_probe.exe` exit 0 — 두 구성, 신규 probe 포함
4. `scripts/task367_hle_boundary_opcode_attribution.ps1 -Runs 3 -DurationSeconds 60`
   * 설계 §7의 N1~N5를 script가 검사하고 위반 시 throw
5. 설계 §6의 A1~A3 중 하나를 판정하고 제거 상한을 프레임으로 환산해 기록

### 4. 완료 조건

* 두 구성 빌드와 probe 통과
* N1~N5 통과
* `0F` 119,235건의 정체가 명령 단위로 확정됨
* A1~A3 판정과 다음 작업 대상이 문서에 기록됨

---

## English

### Scope

Instrumentation only: no instruction semantics, HLE boundary, or behaviour change.
A new census module skips legacy prefixes to record the effective opcode, records
the second byte of `0F`-escaped instructions, and counts prefix presence, while the
existing `bytes[0]` histogram stays for continuity. Prefix skipping is bounded to
four and respects the byte length so a malformed sequence cannot run away, with
`prefix_overflow` and `escape_truncated` counters for the edges. No allocation,
string building, sorting, or clock reads on the hot path; sorting and formatting
happen at exit only.

### Verification

Both builds and the probe suite must pass, then
`scripts/task367_hle_boundary_opcode_attribution.ps1 -Runs 3 -DurationSeconds 60`
must satisfy N1 through N5. N1 — the effective-opcode histogram summing to the same
total as the existing `bytes[0]` histogram — is the one that proves prefix skipping
loses no samples. The run must identify what the 119,235 `0F` instructions actually
are, decide A1, A2, or A3, and record the removal ceiling converted to frames.
