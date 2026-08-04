# Task 412 작업 지시 — 멈춤의 host 시간 귀속

설계: [20260804-412](../design/20260804-412-stall-host-time-attribution.md)

## 변경 파일

| # | 파일 | 변경 |
|---|---|---|
| 1 | `include/repiu/platform/win32/guest_position_census.h` | 스레드 CPU 시간, host 호출 지점 표(1,024), 스캔 검산 counter, 모듈·심볼 해석 API |
| 2 | `src/platform/win32/telemetry/guest_position_census.cpp` | 위 구현. 모듈 해석(`GetModuleHandleExA`)과 `dbghelp` 심볼화, 검산 |
| 3 | `src/platform/win32/native_phase_sampler.h/.cpp` | 정지 중 얕은 스택 훑기(SEH, C++ 객체 없는 함수). 모듈 범위를 받지 않으면 기존 동작 그대로 |
| 4 | `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 로더 모듈 범위 전달, `GetThreadTimes` 기록, host 표본의 호출 지점 기록 |
| 5 | `src/host/win32/main.cpp` | CPU 시간·모듈·호출 지점·스캔 검산 로그 |
| 6 | `CMakeLists.txt` | Release에 `/Zi` + `/DEBUG`, 로더에 `dbghelp` 링크 |

## 구현 규칙

* suspend와 resume 사이에서는 **스택 읽기 외에 아무것도 추가하지 않습니다.** 할당·락
  금지는 그대로입니다.
* 스택 훑기는 실패해도 실행에 영향을 주지 않아야 합니다. SEH로 감싸고 counter만
  올립니다.
* `/Zi`는 **코드 생성을 바꾸지 않는** 디버그 정보 옵션만 켭니다. 최적화 플래그는
  건드리지 않습니다.
* 심볼화는 종료 경로에서만, 게스트 스레드가 멈춘 뒤에만 수행합니다.
* census가 꺼져 있으면 새 경로에 진입하지 않습니다.

## 검증

1. 편집한 번역 단위를 `cl /Zs`로 **문법 검사**해 40분 빌드를 낭비하지 않습니다.
2. Release 빌드 오류 0. PDB 생성 확인.
3. census OFF 실행 1회 — 새 로그 줄이 0/false로 나오고 기존 동작 불변.
4. census ON 실행 — 검산 `sited + no_site + failed == host` 성립, `overflow` 0.
5. CPU 비율로 바쁨/막힘 판정 후 상위 호출 지점 해석(설계 §3).

## 산출물

작업 로그, [pumpit3 기동 중 멈춤](../analysis/pumpit3-startup-stall.md) 갱신,
frontier 항목 0a 갱신, `ARCHITECTURE.md`의 census 절 확장.

---

# Task 412 Work Order — attributing the stall's host time

Design: [20260804-412](../design/20260804-412-stall-host-time-attribution.md)

## Files

The census header and source gain the thread CPU time, a 1,024-entry host call-site table,
scan reconciliation counters, and module/symbol resolution through `GetModuleHandleExA` and
`dbghelp`; the native phase sampler gains a suspended-thread shallow stack scan written in
an SEH function with no C++ objects, inert when no module range is supplied; the poll loop
passes the loader module range, records `GetThreadTimes`, and records call sites for host
samples; `main.cpp` prints CPU time, modules, call sites, and the scan reconciliation; and
`CMakeLists.txt` adds `/Zi` with `/DEBUG` to Release plus the `dbghelp` link.

## Implementation rules

Nothing but the stack read is added between suspend and resume, and the no-allocation,
no-lock rule stands. A failed scan must not affect execution: it is wrapped in SEH and only
bumps a counter. `/Zi` adds debug information only and no optimisation flag changes.
Symbolisation runs on the teardown path after the guest thread has stopped. With the census
off, none of the new paths are entered.

## Verification

Syntax-check the edited translation units with `cl /Zs` before spending forty minutes on a
build; then a zero-error Release build with a PDB; one census-off run showing the new lines
at zero or false and behaviour unchanged; one census-on run where
`sited + no_site + failed == host` and `overflow` is zero; then the busy-or-blocked verdict
from the CPU share, followed by the call-site reading rules from the design.

## Deliverables

The work log, an updated stall analysis, frontier item 0a, and an extended census section
in `ARCHITECTURE.md`.
