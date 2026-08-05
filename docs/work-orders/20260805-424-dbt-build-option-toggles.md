# Task 424 작업 지시 — DBT build option 3종에 toggle 부여

설계: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) §3

**이 작업은 Task 425의 선결 조건입니다.** 이 toggle이 없으면 축소가 진단 축 하나를
없앱니다. Task 425는 이 작업의 완료 기준이 충족된 뒤에 시작합니다.

## 1. 새 모듈 — `runtime::env_toggle`

새 파일 두 개를 만듭니다.

```
include/repiu/runtime/env_toggle.h
src/runtime/env_toggle.cpp
```

```cpp
namespace repiu::runtime
{
// 승격된 기본값: 미지정과 빈 값은 ON. 알 수 없는 값은 fail-closed OFF.
bool ResolvePromotedToggle(const char* value);

// opt-in: 미지정과 빈 값은 OFF. 알 수 없는 값도 OFF.
bool ResolveOptInToggle(const char* value);
}
```

진리표는 다음과 같습니다. `nullptr`과 `""`만 두 함수가 다르게 취급합니다.

| 입력 | `ResolvePromotedToggle` | `ResolveOptInToggle` |
|---|---|---|
| `nullptr`, `""` | `true` | `false` |
| `"1"`, `"on"`, `"true"` | `true` | `true` |
| `"0"`, `"off"`, `"false"` | `false` | `false` |
| 그 외 (`"yes"`, `"2"`, `"ON"` 등) | `false` | `false` |

대소문자 변환은 하지 않습니다. 기존 6개 호출 지점이 정확히 소문자 세 값만 받아들이고
있었으므로, 여기서 관대해지면 기존 실행 절차의 의미가 조용히 바뀝니다.

`CMakeLists.txt`에 `src/runtime/env_toggle.cpp`를 추가합니다.

## 2. probe 추가

```
src/tools/aot_probe/env_toggle_probe.cpp
src/tools/aot_probe/env_toggle_probe.h
```

§1 진리표의 **모든 행을 두 함수 각각에 대해** 확인하고
`env_toggle_policy=true|false` 한 줄을 출력합니다. `execution_backend_probe.cpp`의
형식을 그대로 따릅니다. `src/tools/aot_probe/main.cpp`에 등록하고 `CMakeLists.txt`
probe 소스 목록에 추가합니다.

## 3. 세 build option을 toggle로 전환

`src/host/win32/main.cpp`의 다음 세 줄을 바꿉니다.

```cpp
// 이전
aot_build_options.enable_dbt_return_miss_dispatch =
    execution_backend == repiu::runtime::ExecutionBackend::kAotDbt;
aot_build_options.enable_dbt_direct_edge_dispatch =
    execution_backend == repiu::runtime::ExecutionBackend::kAotDbt;
aot_build_options.enable_timer_safe_points =
    execution_backend == repiu::runtime::ExecutionBackend::kAotDbt;
```

각각 아래 형태로 전환하고, **다른 backend에서는 여전히 비활성**이라는 기존 성질을
유지합니다.

```cpp
// Task 424: 이 셋은 도입 이래 backend 값으로만 결정돼, aot-dynamic 실행이
// 유일한 A/B 수단이었습니다. Task 425의 backend 축소를 위해 개별 toggle로
// 옮깁니다. 미지정은 ON이고 알 수 없는 값은 fail-closed opt-out입니다.
aot_build_options.enable_dbt_return_miss_dispatch =
    execution_backend == repiu::runtime::ExecutionBackend::kAotDbt &&
    repiu::runtime::ResolvePromotedToggle(
        std::getenv("REPIU_AOT_DBT_RETURN_MISS_DISPATCH"));
```

| 환경 변수 | build option |
|---|---|
| `REPIU_AOT_DBT_RETURN_MISS_DISPATCH` | `enable_dbt_return_miss_dispatch` |
| `REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH` | `enable_dbt_direct_edge_dispatch` |
| `REPIU_AOT_DBT_TIMER_SAFE_POINTS` | `enable_timer_safe_points` |

### 3.1 로그

`enable_timer_safe_points`와 direct-edge는 이미 site 수가 로그에 있습니다.

```
Win32 AOT timer safe points enabled/sites: {}/{}
Win32 AOT-DBT unresolved direct-edge dispatch sites: {}
```

direct-edge 줄에 enabled 값을 함께 싣고, **return-miss dispatch에는 로그 줄이 없으므로
새로 추가**합니다. §5의 판정이 로그만으로 성립해야 합니다.

```
Win32 AOT-DBT return-miss dispatch enabled: {}
```

## 4. 기존 toggle 6곳을 helper로 전환

동작 보존 리팩터링입니다. `main.cpp`에서 다음을 `ResolvePromotedToggle` /
`ResolveOptInToggle` 호출로 바꾸고, 인라인 `std::string` 사본과 비교식을 지웁니다.

| 환경 변수 | 전환 대상 |
|---|---|
| `REPIU_AOT_DBT_PORT_IO_DISPATCH` | `ResolvePromotedToggle` |
| `REPIU_AOT_GUARDED_SEGMENT_POP` | `ResolvePromotedToggle` |
| `REPIU_AOT_GUARDED_SEGMENT_LOAD` | `ResolvePromotedToggle` |
| `REPIU_AOT_GUARDED_SEGMENT_READ` | `ResolvePromotedToggle` |
| `REPIU_AOT_DBT_SUPERBLOCK` | `ResolveOptInToggle` |
| `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH` | `ResolveOptInToggle` |

**각 지점의 Task 번호 주석(384 / 386 / 390 / 291 등)은 반드시 보존합니다.** 승격 근거가
그 주석에만 남아 있습니다.

`aot_dbt_dispatch.cpp`의 지역 `ParseToggle` 계열 helper도 같은 형태이면 새 모듈로
대체합니다. 형태가 다르면(다른 값 집합을 받는다면) **그대로 두고** 이유를 작업 로그에
적습니다.

`REPIU_AOT_DBT_INDIRECT`와 `REPIU_AOT_INDIRECT_CACHE_SLOTS`는 진리값이 아니므로
건드리지 않습니다.

## 5. 검증

### 5.1 빌드와 probe

```powershell
cmd /c scripts\build_win32_x86_release.bat
```

`aot_probe` 전체를 돌려 `env_toggle_policy=true`를 포함한 전 항목 통과를 확인합니다.

### 5.2 기본값 동등성 — 가장 중요한 확인

세 toggle을 **모두 미지정**한 채 `REPIU_EXECUTION_BACKEND=aot-dbt`로 1초 smoke를
돌리고, 변경 직전 커밋의 같은 조건 실행과 다음 줄이 **완전히 일치**해야 합니다.
`REPIU_EEPROM_PATH`는 실행마다 격리합니다.

* `Win32 AOT timer safe points enabled/sites`
* `Win32 AOT-DBT unresolved direct-edge dispatch sites`
* `Win32 AOT guarded segment-load/read enabled/sites`
* `Win32 AOT generation publishes/quarantines` (격리 0)

리팩터링이 6개 기존 toggle의 의미를 바꾸지 않았음을 이 비교가 담보합니다.

### 5.3 toggle별 OFF 1회씩

세 변수를 하나씩 `0`으로 두고 1초 smoke를 돌립니다.

| 설정 | 통과 조건 |
|---|---|
| `REPIU_AOT_DBT_RETURN_MISS_DISPATCH=0` | 새 로그 줄이 `false` |
| `REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH=0` | direct-edge site 수 **0** |
| `REPIU_AOT_DBT_TIMER_SAFE_POINTS=0` | timer safe point site 수 **0** |

**설계 §7대로, OFF에서 실행이 중단되더라도 이 작업은 실패가 아닙니다.** 그 경우
어느 toggle에서 어떤 증상이 났는지를 작업 로그와
[frontier](../analysis/current-execution-frontier.md)에 **새로 발견된 의존성**으로
기록하고, toggle 자체는 그대로 남깁니다.

### 5.4 알 수 없는 값

`REPIU_AOT_DBT_TIMER_SAFE_POINTS=yes`로 1회 돌려 site 수가 **0**임을 확인합니다.
fail-closed 규칙이 실제로 적용되는지 보는 확인입니다.

## 6. 완료 기준

1. `env_toggle` 모듈과 probe가 §1 진리표 전 행을 통과합니다.
2. 기본값 실행이 §5.2에서 변경 전과 로그 동일합니다.
3. 세 toggle의 OFF가 §5.3 표대로 관측됐습니다(중단 시 그 사실이 기록됐습니다).
4. §5.4의 fail-closed가 확인됐습니다.
5. `ARCHITECTURE.md`의 환경 변수 목록에 새 변수 3개가 반영됐습니다.
6. 작업 로그를 남겼습니다.

---

# Task 424 Work Order — toggles for three DBT build options

Design: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) §3.

**This is a prerequisite for Task 425**, which must not start until these completion criteria
are met — without the toggles, consolidation removes a diagnostic axis.

## 1. New `runtime::env_toggle` module

Add `include/repiu/runtime/env_toggle.h` and `src/runtime/env_toggle.cpp` exposing
`ResolvePromotedToggle` (unset and empty mean ON) and `ResolveOptInToggle` (unset and empty
mean OFF). Both accept only `1|on|true` as ON and `0|off|false` as OFF, and both treat every
other value — including differently cased ones — as OFF. **No case folding**: the six existing
call sites accept exactly the three lowercase spellings, and relaxing that here would silently
change the meaning of existing run procedures. Register the source in `CMakeLists.txt`.

## 2. Probe

Add `env_toggle_probe.{h,cpp}` covering **every row of the truth table for both functions**,
printing one `env_toggle_policy=` line in the style of `execution_backend_probe.cpp`, and wire
it into `src/tools/aot_probe/main.cpp` and `CMakeLists.txt`.

## 3. Convert the three build options

In `src/host/win32/main.cpp`, extend each of the three `== kAotDbt` assignments with the
matching `ResolvePromotedToggle(std::getenv(...))` call — `REPIU_AOT_DBT_RETURN_MISS_DISPATCH`,
`REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH`, and `REPIU_AOT_DBT_TIMER_SAFE_POINTS` — keeping the
existing property that other backends leave them off, and carrying a comment that names why
the toggle exists. Timer safe points and direct-edge already log their site counts; add the
`enabled` value to the direct-edge line and **add a new line for return-miss dispatch**, which
has none, so that §5's verdicts rest on the log alone.

## 4. Convert the six existing toggles

Behaviour-preserving refactor: route `REPIU_AOT_DBT_PORT_IO_DISPATCH`,
`REPIU_AOT_GUARDED_SEGMENT_POP`, `REPIU_AOT_GUARDED_SEGMENT_LOAD`, and
`REPIU_AOT_GUARDED_SEGMENT_READ` through `ResolvePromotedToggle`, and
`REPIU_AOT_DBT_SUPERBLOCK` and `REPIU_AOT_DBT_SEGMENT_OVERRIDE_DISPATCH` through
`ResolveOptInToggle`, deleting the inline `std::string` copies. **Preserve every Task-number
comment (384 / 386 / 390 / 291)** — the promotion evidence survives only there. Replace the
local toggle helper in `aot_dbt_dispatch.cpp` if it has the same shape; if it accepts a
different value set, leave it and record why in the work log. `REPIU_AOT_DBT_INDIRECT` and
`REPIU_AOT_INDIRECT_CACHE_SLOTS` are not boolean and are out of scope.

## 5. Verification

Build with `scripts\build_win32_x86_release.bat` and run the full `aot_probe`, expecting
`env_toggle_policy=true`.

**The critical check is default equivalence:** with all three variables unset, a one-second
`aot-dbt` smoke (isolated `REPIU_EEPROM_PATH`) must match the pre-change commit exactly on the
timer-safe-point, direct-edge, guarded segment-load/read, and generation publish/quarantine
lines. That comparison is what guarantees §4 changed no meaning.

Then run each toggle OFF once, expecting the return-miss line to read `false` and the
direct-edge and timer-safe-point site counts to reach zero. **Per design §7, execution halting
under OFF does not fail this task**; record which toggle produced which symptom in the work log
and in [frontier](../analysis/current-execution-frontier.md) as a newly discovered dependency
and keep the toggle. Finally set `REPIU_AOT_DBT_TIMER_SAFE_POINTS=yes` once and confirm the
site count is zero, proving the fail-closed rule applies.

## 6. Completion

The module and probe pass the full truth table; the default run is log-identical to the
pre-change build; the three OFF runs were observed (or their halts recorded); fail-closed is
confirmed; `ARCHITECTURE.md`'s environment-variable list carries the three new names; and the
work log is written.
