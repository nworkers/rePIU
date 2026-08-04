# Task 411 작업 지시 — 멈춘 실행의 게스트 위치 census

설계: [20260804-411-stall-guest-position-census.md](../design/20260804-411-stall-guest-position-census.md)

## 범위

1. 정적 분석 결과 문서화(코드 변경 없음) — 지연 루틴의 유일 호출처가 타이머 슬롯
   콜백임을 [pumpit3 기동 중 멈춤](../analysis/pumpit3-startup-stall.md)에 반영.
2. 시간 기준 게스트 위치 census 구현.
3. Release 빌드 후 pumpit3 실측(멈춤 1회 이상 + 정상 1회 이상).

**범위 밖:** 멈춤의 수정, 타이머 주입 경로 변경(설계 §6), 지연 루프 최적화.

## 변경 파일

| # | 파일 | 변경 |
|---|---|---|
| 1 | `include/repiu/platform/win32/guest_position_census.h` (신규) | `Win32GuestPositionCensus`, `Win32GuestPositionCensusSnapshot`, 기록·스냅샷·덤프 API, 환경 변수 해석 |
| 2 | `src/platform/win32/telemetry/guest_position_census.cpp` (신규) | 위 구현. open-addressing 표(4,096), origin 분류, 덤프 작성 |
| 3 | `CMakeLists.txt` | 새 소스 추가 |
| 4 | `src/platform/win32/execution/thread_context.h` | `std::unique_ptr<Win32GuestPositionCensus> guest_position_census` |
| 5 | `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 폴링 루프에서 dispatch-quiet 게이트 **없이** 간격 표본. 스냅샷·덤프를 attempt에 채움 |
| 6 | `include/repiu/platform/win32/execution_trampoline.h` | `Win32MinimalExecutionAttempt`에 census 스냅샷 필드 |
| 7 | `src/platform/win32/execution/execution_trampoline.cpp` | census 활성 시 ThreadContext에 할당 |
| 8 | `src/host/win32/main.cpp` | 로그 출력(총계·분류·상위 16) |

## 구현 규칙

* 게스트 스레드는 **캡처 순간 외에는 건드리지 않습니다.** suspend와 resume 사이에서
  할당·락·I/O를 하지 않습니다(기존 `CaptureWin32NativePhaseSample` 계약 유지).
* census가 꺼져 있으면 표본 경로에 진입하지 않습니다. 기본 동작은 불변이어야 합니다.
* `REPIU_NATIVE_SAMPLING` 경로와 그 dispatch-quiet 게이트는 **수정하지 않습니다.**
* 분류 합이 total과 같도록 기록 지점을 하나로 유지합니다(설계 §5 검산).
* 덤프는 게스트 스레드 정지 직후, Glide close 전에 씁니다.

## 검증

1. `cmake --build build/win32_x86_debug --config Release` 오류 0.
2. census OFF로 pumpit3 45초 1회 — 기존 로그 항목이 그대로 나오고 census 줄은
   `enabled=false`.
3. census ON으로 pumpit3 45~60초 반복 — 멈춤 1회 이상, 정상 1회 이상 확보
   ([재현 가이드](../guides/pumpit3-stall-reproduction.md)의 `DOS path trace #` 6줄 판정).
4. 검산: `arena+cache+cache_unmapped+host == total`, `overflow == 0`.
5. 상위 표를 두 실행 사이에서 비교하고, 멈춤 쪽 상위 주소를
   `repiu_aot_probe --dump`(주소 `-0x02000000`)로 역어셈블합니다.

## 산출물

* 작업 로그 `docs/work-logs/20260804-411-stall-guest-position-census.md`
* [pumpit3 기동 중 멈춤](../analysis/pumpit3-startup-stall.md) 갱신(정적 사슬 + 측정 결과)
* [실행 정지 지점 EIP census 가이드](../guides/execution-stall-eip-census.md)에 새 절차 추가
* `ARCHITECTURE.md`에 새 하위 시스템 반영, frontier 갱신

---

# Task 411 Work Order — a census of where the guest is during the stall

Design: [20260804-411-stall-guest-position-census.md](../design/20260804-411-stall-guest-position-census.md)

## Scope

Document the static result (the delay routine's only caller is a timer slot callback) in
the [stall analysis](../analysis/pumpit3-startup-stall.md); implement the time-based guest
position census; build Release and measure pumpit3 with at least one stalled and one
healthy run. **Out of scope:** fixing the stall, changing the timer injection path (design
§6), and optimising the delay loop.

## Files

New `include/repiu/platform/win32/guest_position_census.h` and
`src/platform/win32/telemetry/guest_position_census.cpp` hold the census type, its record,
snapshot, dump, and environment helpers; `CMakeLists.txt` gains the source;
`thread_context.h` gains the owning pointer; `live_telemetry_snapshot.cpp` samples on an
interval **without** the dispatch-quiet gate and fills the attempt snapshot and dump;
`execution_trampoline.h` gains the snapshot field; `execution_trampoline.cpp` allocates the
census when enabled; `main.cpp` prints totals, the origin split, and the top sixteen.

## Implementation rules

Nothing between suspend and resume may allocate, lock, or perform I/O, keeping the existing
`CaptureWin32NativePhaseSample` contract. With the census off, the sampling path is not
entered and behaviour is unchanged. The `REPIU_NATIVE_SAMPLING` path and its dispatch-quiet
gate are not modified. Keep one recording site so the origin counts sum to the total. Write
the dump immediately after the guest thread stops, before Glide close.

## Verification

Release build with zero errors; one census-off run to show the default path is unchanged;
repeated census-on runs until at least one stalled and one healthy run are captured, judged
by the six `DOS path trace #` lines from the
[reproduction guide](../guides/pumpit3-stall-reproduction.md); the `sum == total` and
`overflow == 0` checks; then compare the top tables and disassemble the stalled run's top
addresses with `repiu_aot_probe --dump` at `address - 0x02000000`.

## Deliverables

The work log, the updated stall analysis, a new procedure section in the
[EIP census guide](../guides/execution-stall-eip-census.md), and updates to
`ARCHITECTURE.md` and the execution frontier.
