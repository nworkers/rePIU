# Task 435 작업 지시 — 실행 기본값을 `dynamic` · 무제한으로

설계: [20260807-435](../design/20260807-435-execution-default-policy.md)

## 1. 범위

`REPIU_EXECUTION_BACKEND`의 기본값을 **`dynamic`**, `REPIU_EXECUTION_TIMEOUT_MS`의
기본값을 **`0`(무제한)** 으로 바꿉니다. **새 실행 경로를 만들지 않습니다** — 두 값 모두
이미 존재하는 선택지이고, 미지정일 때 고르는 값만 바뀝니다.

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `include/repiu/runtime/execution_backend.h` · `src/runtime/execution_backend.cpp` | `kDefaultExecutionBackend = kDynamic`, `ResolveExecutionBackend(const char*, ExecutionBackend*)` 추가 |
| `include/repiu/runtime/execution_timeout.h` · `src/runtime/execution_timeout.cpp` (신규) | `kUnlimitedExecutionTimeoutMilliseconds = 0`, `kDefaultExecutionTimeoutMilliseconds = 0`, `ResolveExecutionTimeoutMilliseconds(const char*)` |
| `src/host/win32/main.cpp` | 두 resolver 사용. host에는 `0` → `INFINITE` 매핑만 남김 |
| `CMakeLists.txt` | 새 runtime source와 probe source 등록 |
| `src/tools/aot_probe/execution_backend_probe.cpp` | 미설정·빈 값 → `dynamic` 단정 추가 |
| `src/tools/aot_probe/execution_timeout_probe.{h,cpp}` (신규) · `main.cpp` | 기본·`0`·양수·파싱 실패 단정 |
| `scripts/test_all.ps1` | `legacy` + `1000` ms 명시 고정 |
| `scripts/test_openwatcom_samples.ps1` | `-Backend` · `-GuestTimeoutMilliseconds` 매개변수(기본 `legacy` · `1000`) |
| `README.md` · `ARCHITECTURE.md` · `docs/guides/gameplay-scene-capture.md` | 기본값 서술 갱신, README에 backend 항목 추가 |

**건드리지 않을 것:** backend 해석 규칙(옛 이름 거부 포함), timeout 파싱 방식,
supervisor(이미 `0`을 명시), 가이드·스크립트의 명시 설정, OpenWatcom baseline JSON.

## 3. 구현 규칙

* **`legacy`를 opt-out으로 남깁니다.** 회귀 대조군이며, 이분(二分) 진단의 유일한 수단입니다.
* **기본값 결정을 runtime에 둡니다.** host 익명 namespace에 있으면 probe가 닿지 않습니다.
  `INFINITE`은 Win32 대기 API 값이므로 host에 남깁니다.
* **회귀 harness는 제품 기본값을 따라가지 않습니다.** baseline이 기록된 값을 스크립트에
  명시하고, 그 이유를 주석으로 남깁니다. 기본값을 따라가면 `tests/baselines`의 의미가
  코드 변경 없이 바뀝니다.
* 잘못된 backend 값은 지금처럼 **오류 종료**를, 잘못된 timeout 값은 **기본값 복귀**를
  유지합니다(설계 §4.2의 비대칭과 그 근거).

## 4. 검증

1. Win32 Debug 빌드 통과.
2. `repiu_aot_probe.exe` 기본 실행 통과 — `execution_backend_policy=true`,
   `execution_timeout_policy=true`.
3. 환경 변수를 지운 짧은 스모크에서 `Win32 requested execution backend: dynamic`,
   `Win32 guest execution timeout: disabled`.
4. `REPIU_EXECUTION_TIMEOUT_MS=1000` 스모크가 이전처럼 1초에 종료.

## 5. 완료 기준

1. 아무 설정 없이 로더를 켜면 `dynamic`으로, 시간 제한 없이 실행됩니다.
2. `REPIU_EXECUTION_BACKEND=legacy`와 `REPIU_EXECUTION_TIMEOUT_MS=N`이 그대로 동작합니다.
3. 두 회귀 harness가 기본값 변경과 무관하게 이전과 같은 조건에서 돌아갑니다.
4. README·ARCHITECTURE·가이드의 기본값 서술이 코드와 일치합니다.

---

# Task 435 Work Order — default to `dynamic` and to no time limit

## 1. Scope

`REPIU_EXECUTION_BACKEND` defaults to **`dynamic`** and `REPIU_EXECUTION_TIMEOUT_MS` to **`0`,
meaning unlimited**. **No new execution path** — both values already exist; only the choice made
when nothing is set changes.

## 2. Files

`execution_backend.{h,cpp}` gain `kDefaultExecutionBackend` and `ResolveExecutionBackend`; a new
`execution_timeout.{h,cpp}` carries the unlimited sentinel, the default and
`ResolveExecutionTimeoutMilliseconds`; `src/host/win32/main.cpp` calls both and keeps only the
`0` → `INFINITE` mapping; `CMakeLists.txt` registers the new runtime and probe sources; the
backend probe gains an unset-default assertion and a new timeout probe covers its four branches;
`scripts/test_all.ps1` and `scripts/test_openwatcom_samples.ps1` pin `legacy` and `1000` ms; and
`README.md`, `ARCHITECTURE.md` and the gameplay capture guide are corrected, with the README
gaining its missing `REPIU_EXECUTION_BACKEND` entry.

**Not touched:** the backend parsing rules including the rejection of the retired names, the
timeout parsing itself, the supervisor (which already sets `0`), the explicit settings in guides
and scripts, and the OpenWatcom baseline JSON.

## 3. Implementation rules

Keep `legacy` as the opt-out: it is the regression control and the only way to bisect. Put the
default decision in the runtime layer, since a host anonymous namespace is out of a probe's
reach, while `INFINITE` stays in the host because it belongs to the Win32 wait API. **The
regression harnesses do not follow the product default** — they pin the values their baselines
were recorded under, with the reason in a comment, because following the default would change
what `tests/baselines` means with no code change behind it. An invalid backend still exits
loudly; an invalid timeout still falls back to the default.

## 4. Verification

The Debug build passes; `repiu_aot_probe.exe` reports `execution_backend_policy=true` and
`execution_timeout_policy=true`; a short smoke with the environment cleared logs
`Win32 requested execution backend: dynamic` and `Win32 guest execution timeout: disabled`; and
`REPIU_EXECUTION_TIMEOUT_MS=1000` still ends after one second.

## 5. Done when

Launching the loader with nothing set runs `dynamic` with no time limit, `legacy` and an
explicit budget still work, both regression harnesses run under the same conditions as before,
and the documented defaults match the code.
