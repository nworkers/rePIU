# AOT-DBT return miss host dispatch 분석 / AOT-DBT return-miss host dispatch

## 한국어

### 확인됨

Task 277은 `aot-dbt`의 translated `C3`/`C2 iw` return inline-cache miss를
정상 host-stack call로 처리합니다. 기존 `aot-dynamic`과 다른 backend의 miss tail은
계속 `popfd; INT3`입니다.

guest stack에서 C++를 실행하지 않기 위해 naked x86 thunk가 register/EFLAGS를 먼저
저장하고, entry trampoline이 기록한 host ESP와 TEB stack base/limit로 전환합니다.
기존 `HandleAotReturnTransfer`가 target resolution, dynamic append, return telemetry와
serialized inline-cache patch를 그대로 수행합니다. 성공 시 stack continuation의
`ret imm16`이 원본 return pop과 cache 이동을 재현합니다. 실패 시 `LEA ESP`가 DBT
metadata만 제거하고 provenance `INT3`가 기존 VEH 경로를 실행합니다.

최초 실제 실행은 시도/성공/fallback `39,296/0/39,296`을 기록했습니다. guest 상태는
손상되지 않았지만, 기존 return handler가 요구하는 `aot_reentry_pending` 진입 계약을
정상-call adapter가 설정하지 않아 모두 fail-closed한 것이 원인이었습니다. 이 상태를
handler 호출 전에 설정한 뒤 성공 경로가 활성화됐습니다.

최종 15초 `pumpit1` 실행은 다음을 기록했습니다.

| 지표 | `aot-dynamic` | `aot-dbt` |
|---|---:|---:|
| DBT return 시도/성공/fallback | 0/0/0 | 5,507/849/4,658 |
| AOT boundary/re-entry | 17,781/17,816 | 17,662/18,546 |
| progress | 12,745 | 13,251 |
| fatal / legacy fallback | 0 / 0 | 0 / 0 |

두 실행은 단일 표본이고 초기화 timing이 다르므로 progress 차이를 성능 향상으로
해석하지 않습니다. 확인 가능한 국소 효과는 성공 849회가 각각 return miss의
`INT3`/VEH 왕복 하나를 제거했다는 것입니다. 두 격리 EEPROM은 원본 SHA-256
`A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`을 유지했습니다.

### 미확정

- fallback 4,658회의 target 분류(quarantine, HLE target, dynamic miss 등)
- return host dispatch의 안정된 hot-phase wall-clock 효과
- 같은 host-stack ABI를 indirect call/jump miss에 일반화할 때 필요한 operand capture

## Task 482 pass 2 — return 한 번의 비용 분해 (2026-08-22) — **확인됨, 3회 재현**

pumpit8 Release, vsync OFF, `REPIU_AOT_RETURN_STAGE_PROFILE=1` +
`REPIU_EXECUTION_TIME_PROFILE=1`로 사용자가 3회 실행(`pass4~6.txt`, 33.3k~52.5k 프레임)한
로그입니다. 건전성은 세 실행 모두 clamp `0/0`, `scans=0`, return fallback 0입니다.

### 확인됨 1 — return 비용은 알려진 것보다 1.39배 큽니다

| 지표 | pass4 | pass5 | pass6 |
|---|---:|---:|---:|
| outer 창 (`guest-run` 대비) | 35.76% | 36.44% | 36.38% |
| 기존 `kAotReturn` bucket | 25.63% | 26.18% | 26.07% |
| outer / bucket | **1.396** | **1.392** | **1.395** |
| 계측 제외 실비용 (covered) | 26.86% | 27.44% | 27.32% |

`kAotReturn`은 `HandleAotReturnTransfer` 안만 재므로 `ResolveAotDbtReturnMissFrame`의
site 조회·frame marshalling·frame writeback이 빠져 있었습니다. **frontier가 16.5~17.5%로
알고 있던 return 비용은 실제로 `guest-run`의 약 27%입니다**(계측 오버헤드 제외).
2026-08-13 세션이 "핸들러가 VEH 밖으로 옮겨가 사각지대가 생겼다"고 한 것과 같은 형태가
한 겹 더 있었습니다.

### 확인됨 2 — 지배적인 단계가 없습니다

return 하나는 약 1,490~1,514 cycle(계측 포함), 약 1,120~1,140 cycle(계측 제외)입니다.

| stage | covered 대비 (3회) | return당 | 내용 |
|---|---:|---:|---|
| `resolve` | 31.75 / 31.41 / 31.74% | 약 356 | 분류 + `ResolveAotTransferTarget` |
| `patch` | 27.54 / 28.22 / 27.55% | 약 313 | IC miss 판정 + Task 481 정책 |
| `entry` | 25.07 / 24.95 / 25.08% | 약 282 | site 조회 · RET 검증 · frame marshalling |
| `read` | 8.33 / 8.21 / 8.33% | 약 93 | 게스트 스택 target + bookkeeping |
| `continuation` | 7.31 / 7.21 / 7.31% | 약 82 | ESP/EIP/EFLAGS · writeback |

상위 세 단계가 84%를 차지하고 서로 25~32% 범위 안에 있습니다. 설계
20260814-482의 분기 중 **"여러 stage에 고르게 남는 경우"**에 해당합니다.

`resolve`의 최대값(약 3,400만 cycle)은 실행당 305회뿐인 dynamic translation이 그 창에서
일어난 결과이고, 평균은 `ResolveAotTransferTarget` 자체(호출당 221~226 cycle)가
지배합니다. 평균과 최대를 같은 원인으로 읽으면 안 됩니다.

### 확인됨 3 — `patch` 단계는 "아무것도 하지 않기로 결정"하는 비용입니다

정책 관측 49.6M~71.0M 중 **99.39~99.41%가 bypass**이고 실제 패치는 **0.59~0.62%**뿐인데,
`patch` 단계는 covered의 27.5~28.2%(`guest-run`의 약 7.5%)를 씁니다. Task 481은 패치
**행위**를 없앴지만, 그 판정을 위해 return마다 `IsAotInlineCacheMiss`가 **megamorphic
판정보다 먼저** 돕니다
([aot_runtime_dispatch.cpp](../../src/platform/win32/aot/aot_runtime_dispatch.cpp) return
경로). 이미 megamorphic으로 확정된 site는 이 miss 판정 결과가 항상 같으므로, 정책 상태를
먼저 보고 miss 판정을 건너뛰는 순서 교체가 가장 싼 후보입니다.

**이것은 Glide 축에서 나온 것과 같은 형태의 낭비입니다** — elision과 bypass 모두
"하지 않기로 하는 판정"을 비싼 경로를 지난 뒤에 수행합니다.

### 다음 구현 선택 (작업 지시 8번)

99.4%의 return이 megamorphic site에서 발생하고, 그 return이 하는 일은 결국
guest target → cache target 한 번의 조회입니다. 그 조회 자체(`ResolveAotTransferTarget`)는
호출당 221~226 cycle인데, 거기 도달하고 돌아오는 데 약 900 cycle을 더 씁니다. 따라서
설계가 예비해 둔 **generated megamorphic direct-return table**이 데이터가 가리키는
방향입니다. 상한은 대략 `guest-run`의 15~20%이며, generation·retirement 정확성 경계를
별도 설계로 먼저 정리해야 합니다.

그보다 훨씬 작고 위험이 낮은 선행 후보는 **megamorphic site에서 IC miss 판정 건너뛰기**
(`patch` 단계, `guest-run`의 약 7.5% 상한)입니다.

### 방법 주의

프레임당 패치 수는 8.81 / 8.31 / 8.76으로 pass5가 Task 478의 3% 규칙을 벗어납니다.
그러나 이 분석은 실행 간 비교가 아니라 실행 안 귀속이고, 단계 비중이 세 실행에서
0.5포인트 이내로 재현되므로 결론은 유지됩니다.

## English

**Task 482 pass 2: what one return costs (2026-08-22, confirmed across three runs).** Three user
runs of pumpit8 on Release with vsync off and `REPIU_AOT_RETURN_STAGE_PROFILE=1`
(`pass4-6.txt`, 33.3k-52.5k frames) all reported zero clamps, `scans=0`, and zero return
fallbacks.

*Confirmed 1 - return handling costs 1.39x what was known.* The outer window is 35.76 / 36.44 /
36.38% of `guest-run` against a `kAotReturn` bucket of 25.63 / 26.18 / 26.07%, a ratio of 1.396 /
1.392 / 1.395. The bucket measures only the inside of `HandleAotReturnTransfer`, so the adapter's
site lookup, frame marshalling, and frame writeback were never counted. Excluding the
instrument's own overhead, return handling is **about 27% of `guest-run`**, not the 16.5-17.5%
the frontier carried - the same blind spot the 2026-08-13 session found, one layer further out.

*Confirmed 2 - no stage dominates.* One return costs about 1,490-1,514 cycles instrumented and
about 1,120-1,140 without the instrument, split as `resolve` 31.75/31.41/31.74% of covered (~356
cycles), `patch` 27.54/28.22/27.55% (~313), `entry` 25.07/24.95/25.08% (~282), `read`
8.33/8.21/8.33% (~93), and `continuation` 7.31/7.21/7.31% (~82). The top three are 84% of the
total and sit within 25-32% of each other, which is design 20260814-482's "cost spread across
stages" branch. `resolve`'s ~34-million-cycle maximum comes from the 305 dynamic translations per
run that landed inside that window; its average is dominated by `ResolveAotTransferTarget` itself
at 221-226 cycles per call.

*Confirmed 3 - the `patch` stage is the cost of deciding to do nothing.* Of 49.6M-71.0M policy
observations, 99.39-99.41% are bypasses and only 0.59-0.62% patch, yet the stage holds 27.5-28.2%
of covered cycles, about 7.5% of `guest-run`. Task 481 removed the patching, but
`IsAotInlineCacheMiss` still runs on every return *before* the megamorphic verdict is consulted.
For a site already classified megamorphic that test's answer never changes, so consulting the
policy state first is the cheapest candidate. This is the same shape of waste the Glide axis
showed: both elision and bypass make their "do nothing" decision after paying for the expensive
path.

*Next implementation (work-order item 8).* Since 99.4% of returns happen at megamorphic sites and
all they ultimately do is map one guest target to one cache target - a lookup that costs 221-226
cycles inside `ResolveAotTransferTarget`, surrounded by about 900 cycles of getting there and
back - the data points at the **generated megamorphic direct-return table** the design held in
reserve, with a ceiling around 15-20% of `guest-run` and a generation/retirement correctness
boundary that needs its own design first. The smaller, lower-risk precursor is skipping the
inline-cache miss test at megamorphic sites, bounded by the `patch` stage's ~7.5%.

*Method note.* Patches per frame were 8.81 / 8.31 / 8.76, so pass5 falls outside the Task 478 3%
rule, but this is attribution within each run rather than a comparison between runs, and the
stage shares reproduce to within half a point.

Task 277 routes translated `C3`/`C2 iw` inline-cache misses through a normal
host-stack call only under `aot-dbt`; all existing backends retain `popfd; INT3`.
A naked x86 thunk saves registers/EFLAGS, switches to the entry trampoline's
saved host ESP and TEB bounds, and reuses `HandleAotReturnTransfer` for target
resolution, dynamic append, telemetry, and serialized patching. Success uses a
`ret imm16` continuation to preserve the original stack effect; failure removes
only DBT metadata with `LEA ESP` and reaches the established provenance `INT3`.

The first live run safely failed closed for all 39,296 attempts because the
normal-call adapter had not established the return handler's existing
`aot_reentry_pending` entry contract. Setting that state before the shared
handler enabled the success path.

The final controlled 15-second runs recorded 5,507/849/4,658 DBT return
attempts/successes/fallbacks for `aot-dbt` and 0/0/0 for `aot-dynamic`, with zero
fatal state, zero legacy fallback, and unchanged isolated EEPROM hashes. Timing
differences make the progress totals unsuitable as a performance claim; the
local confirmed effect is one avoided `INT3`/VEH round trip per 849 successes.
Fallback classification, stable hot-phase wall-clock impact, and generalization
to indirect call/jump operand capture remain open.
