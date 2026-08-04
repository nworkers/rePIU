# Task 418 작업 지시 — 비용 프로파일 재기준선

설계: [20260804-418](../design/20260804-418-cost-profile-rebaseline.md) ·
절차 원본: [port I/O / arena 귀속 가이드](../guides/port-io-arena-attribution.md) ·
[실행 정지 지점 EIP census 가이드](../guides/execution-stall-eip-census.md)

**이 작업은 측정입니다.** 코드 변경은 계측 공백이 판정을 막을 때만 하고, 그때도
동작을 바꾸지 않는 계측에 한정합니다.

## 0. 선결 — Release 재빌드 (건너뛰면 측정이 무의미합니다)

현재 `build/win32_x86_debug/Release/repiu_loader_win32.exe`에는
`REPIU_PORT_IO_DELAY_LOOP`, `REPIU_AOT_STRICT_SPANNING_ENTRY` 문자열이 **없습니다**.
즉 Tasks 414~417 **이전** 바이너리입니다.

```powershell
cmd /c scripts\build_win32_x86_release.bat
```

빌드 후 확인 — 두 문자열이 모두 나와야 합니다.

```powershell
Select-String -Path build\win32_x86_debug\Release\repiu_loader_win32.exe `
  -Pattern "REPIU_AOT_STRICT_SPANNING_ENTRY" -Encoding Byte -List
```

## 1. 공통 조건

* 같은 빌드, **같은 세션**(세션 간 절대 비교는 성립하지 않습니다)
* `REPIU_EXECUTION_BACKEND=aot-dbt`, `REPIU_EXECUTION_TIMEOUT_MS=60000`
* **EEPROM을 실행마다 격리**합니다 — `REPIU_EEPROM_PATH=<실행별 사본>`
* 출력은 **`cmd /c` 리다이렉션**으로 받습니다. PowerShell 리다이렉션은 콘솔 폭
  120자에서 census 줄을 자릅니다

## 2. 그룹 A — 인용 가능 실행 (pumpit3 5회, pumpit1 2회)

```
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=60000
set REPIU_EXECUTION_TIME_PROFILE=1
set REPIU_EEPROM_PATH=<실행별 사본>
cmd /c "build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit3 > a3-1.txt 2>&1"
```

`REPIU_PORT_IO_CENSUS_MAPPING`과 `REPIU_GUEST_POSITION_CENSUS`는 **끕니다**
(설정하지 않습니다). pumpit1 2회는 같은 조건에서 타깃만 바꿉니다 → `a1-1.txt`, `a1-2.txt`.

## 3. 그룹 B — 호스트 분해 실행 (pumpit3 2회, 시간 인용 금지)

```
set REPIU_GUEST_POSITION_CENSUS=1
set REPIU_GUEST_POSITION_CENSUS_MS=10
cmd /c "build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit3 > b3-1.txt 2>&1"
```

## 4. 실행마다 먼저 볼 것 — 검산 (깨지면 그 실행은 표본에서 제외)

| 확인 | 통과 조건 |
|---|---|
| `Win32 AOT generation publishes/quarantines` | 격리 수 **0** |
| `Win32 AOT generation failure addresses/skips/quarantine-fallbacks/spanning-activations` | 실패 주소 **0** |
| `_GRBUFFERSWAP@4 count` | **≥ 800** |
| `DOS path trace #` 개수 | **≥ 8** |
| `Win32 arena single-step exit total/sum` | **총수 == 합** |
| 진입 분류 수 | 해당 예외 총수 **이하** |
| (그룹 B) host 표본 | `sited + no-site + failed == 총 표본`, `overflow` 0 |
| `Runtime memory arena base` | 실행 간 동일. 다르면 주소 비교 전 오프셋 보정 |

부팅 크래시(frontier 항목 8)가 나면 **표본에서 빼되 횟수는 기록**합니다.

## 5. 채울 표

**표 1 — 실행 기준선(그룹 A, 중앙값과 범위)**

| 타깃 | 프레임 | wall(ms) | 예외 총수 | single-step | breakpoint | AV | other |
|---|---:|---:|---:|---:|---:|---:|---:|
| pumpit3 (5회) | | | | | | | |
| pumpit1 (2회) | | | | | | | |

**표 2 — port I/O 주소 census(pumpit3만, 상위 5개)**

| # | guest | count | cache | arena | 전체 대비 |
|---:|---|---:|---:|---:|---:|

**Task 414 이전 대비:** `0x0301DB22`가 port I/O의 85.9~97.2%였습니다. 이번 값과
**반드시 함께** 적습니다.

**표 3 — 예외 인구(boundary opcode census, 기계별)**

| 기계 | 표본 대비 | 이전 값(Task 367) |
|---|---:|---:|
| Glide gate UD2 | | 55.21% |
| segment register move | | 20.11% |
| port I/O | | 13.15% |

**표 4 — host 호출 지점(그룹 B, 상위 8개 심볼)**

| # | 심볼 | 비중 | Task 412(멈춤) 값 |
|---:|---|---:|---:|

**표 5 — 실행시간 프로파일 device별(그룹 A)** — `kPortIoDevice` count는 과대 계상
이므로 **횟수는 census를 씁니다.**

## 6. 판정 — 설계 §6 결정 트리를 그대로 적용

측정 후에 기준을 바꾸지 않습니다. 트리가 지목한 축 하나만 다음 Task로 넘깁니다.
어느 인구도 30%에 못 미치면 **"단일 축 없음"이 결론**이며, 프레임 예산 분해로
전환합니다.

## 7. 완료 기준

1. 그룹 A 5회 + pumpit1 2회, 그룹 B 2회가 §4 검산을 통과했습니다.
2. 표 1~5가 채워졌고, 각 항목에 **이전 값이 병기**돼 있습니다.
3. 결정 트리가 축 하나(또는 "단일 축 없음")를 지목했습니다.
4. 작업 로그를 쓰고, [frontier](../analysis/current-execution-frontier.md)의 우선순위
   표와 "재측정 필요" 표시를 실측치로 갱신했습니다.
5. Task 414 이후 분포가 바뀐 항목은 [port I/O 가이드](../guides/port-io-arena-attribution.md)의
   전제 주의 문단도 함께 갱신했습니다.

---

# Task 418 Work Order — re-baselining the cost profile

Design: [20260804-418](../design/20260804-418-cost-profile-rebaseline.md). **This task is a
measurement**; code changes only if an instrumentation gap blocks a verdict, and then only
for instrumentation that does not change behaviour.

## 0. Prerequisite — rebuild Release

The current loader contains neither `REPIU_PORT_IO_DELAY_LOOP` nor
`REPIU_AOT_STRICT_SPANNING_ENTRY`, so it predates Tasks 414-417. Run
`cmd /c scripts\build_win32_x86_release.bat` and confirm both strings are present before
measuring anything.

## 1. Common conditions

One build and **one session**, `REPIU_EXECUTION_BACKEND=aot-dbt`,
`REPIU_EXECUTION_TIMEOUT_MS=60000`, a **per-run** `REPIU_EEPROM_PATH`, and output captured
through `cmd /c` redirection — PowerShell truncates census lines at 120 columns.

## 2-3. Groups

**Group A (quotable):** five pumpit3 runs and two pumpit1 runs with
`REPIU_EXECUTION_TIME_PROFILE=1` and both censuses off. **Group B (not quotable for time):**
two pumpit3 runs with `REPIU_GUEST_POSITION_CENSUS=1` and `REPIU_GUEST_POSITION_CENSUS_MS=10`.

## 4. Per-run cross-checks

Zero quarantines, zero generation-failure addresses, at least 800 frames and eight DOS path
traces, `arena single-step exit` sum equal to total, no entry class above its exception
total, and for group B `sited + no-site + failed == total` with zero overflow. Confirm
`Runtime memory arena base` matches across runs before comparing addresses, and drop — but
count — any boot crash.

## 5. Tables to fill

Run baseline by target; the pumpit3 port I/O census top five **beside** the pre-414 figure
that `0x0301DB22` was 85.9-97.2% of port I/O; the boundary-opcode populations beside Task
367's 55.21 / 20.11 / 13.15; the group B host call sites beside Task 412's stalled-run
shares; and the per-device execution-time profile, remembering that the census, not the
profiled `kPortIoDevice` count, gives the number of operations.

## 6-7. Verdict and completion

Apply the design's decision tree unchanged — no criterion is revised after seeing the data —
and carry exactly one axis (or the "no single axis" verdict) into the next task. Done when
all runs pass the cross-checks, every table carries its previous value alongside the new one,
the tree has named an axis, and the work log plus the frontier's priority table and the port
I/O guide's premise note are updated with the measured figures.
