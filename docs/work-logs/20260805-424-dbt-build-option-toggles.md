# Task 424 작업 로그 — DBT build option 3종에 toggle 부여

설계: [20260805-424](../design/20260805-424-execution-backend-consolidation.md) ·
지시: [20260805-424](../work-orders/20260805-424-dbt-build-option-toggles.md)

## 1. 한 일

`runtime::env_toggle` 모듈(`ResolvePromotedToggle`, `ResolveOptInToggle`)을 추가하고,
`main.cpp`에 6번 복제돼 있던 파싱 관례를 그리로 옮겼습니다. backend 값만으로 결정되던
세 build option에 각각 `REPIU_AOT_DBT_*` toggle을 부여했고, 로그로 판정할 수 없던
return-miss dispatch에 전용 줄을 추가했으며 direct-edge 줄에 `enabled` 값을 실었습니다.

`env_toggle_probe`가 두 함수의 진리표 17행을 검증합니다.

## 2. 지시에서 벗어난 것 하나

`aot_dbt_dispatch.cpp`의 `ResolveAotDbtPostHleTranslationEnabled`는 **전환하지
않았습니다.** 지시 §4가 허용한 예외입니다. 값 집합은 같지만 이 함수는 `std::string_view`를
받는 `Resolve*(std::string_view setting)` 계열 18개 중 하나이고, 각자 전용 probe를 갖는
정책 resolver입니다. getenv 결과를 파싱하는 관례와는 다른 층이므로, 하나만 옮기면
형제 함수들과 어긋납니다.

## 3. 검증

### 3.1 빌드와 probe

Release 빌드 exit 0. `repiu_aot_probe MASTER\PIU_1ST\PIU.EXE` exit 0,
`env_toggle_policy=true`와 기존 정책 probe 전 항목 통과.

### 3.2 기본값 동등성 — 변경 직전 커밋(`3909e82`)과 A/B

변경 전 소스를 별도 worktree에서 빌드해 같은 조건으로 1초 pumpit3 smoke를 돌렸습니다.
두 실행 모두 arena base `0x03000000`, EEPROM은 실행별 격리.

| 항목 | baseline | 변경 후 |
|---|---|---|
| indirect inline-cache slots | 4 | 4 |
| guarded segment-pop enabled | true | true |
| guarded segment-load enabled/sites | true/54 | true/54 |
| guarded segment-read enabled/sites | true/43 | true/43 |
| direct-edge sites | 10 | 10 |
| superblock HLE dispatch | false | false |
| Port-I/O dispatch | true | true |
| segment-override dispatch | false | false |
| Glide gate direct dispatch | true | true |
| timer safe points enabled/sites | true/550 | true/550 |
| segment-pop success/fallback | 36/4 | 36/4 |
| segment-load success/fallback | 8/5 | 8/5 |
| generation publishes/quarantines | 0/0 | 0/0 |

**정적 값뿐 아니라 런타임 카운터까지 일치**하므로, 6개 기존 toggle의 의미가 바뀌지
않았습니다. 차이는 새로 추가한 return-miss 줄과 direct-edge 줄의 `enabled` 항목뿐입니다.

### 3.3 toggle별 OFF (pumpit3, 1초)

| 설정 | 결과 |
|---|---|
| `REPIU_AOT_DBT_RETURN_MISS_DISPATCH=0` | `enabled: false`, 실행 계속. direct-edge 10과 timer 550 불변 |
| `REPIU_AOT_DBT_TIMER_SAFE_POINTS=0` | `false/0`, 실행 계속 (generation publishes 4) |
| `REPIU_AOT_DBT_TIMER_SAFE_POINTS=yes` | `false/0` — 알 수 없는 값의 fail-closed 확인 |
| `REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH=0` | **exit 1** — §4 참조 |

측정 중 `TIMER_SAFE_POINTS=0` 실행 하나가 direct-edge site를 8로 보고했는데, 그 실행만
arena base가 `0x07000000`이었습니다. 같은 base(`0x03000000`)로 재실행하니 10으로
일치했습니다. **toggle 영향이 아니라 relocation base 차이**이며, Task 418 지시 §4가
경고한 그 함정입니다.

## 4. 확인됨 — direct-edge dispatch는 이미지에 따라 필수

`REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH=0`은 pumpit3에서 실행 전에 종료합니다.

```
Failed to build requested AOT execution image:
AOT translation plan is ready / direct control-flow target is outside the cache
```

pumpit3 `PIU.EXE`는 cache 밖을 가리키는 direct edge를 10개 갖고, 이 dispatch 없이는
emitter가 그 edge를 표현할 수 없습니다. 같은 edge가 0개인 pumpit1은 끄고도 정상
빌드·실행됩니다(`enabled/sites: false/0`, exit 0).

설계 §7이 예상한 "새로 발견된 의존성"이며, 회귀가 아닙니다. toggle은 그대로 남깁니다 —
함정이 아니라 이미지 의존적이고, 실패가 조용하지 않으며 메시지가 원인을 정확히
지목하고, pumpit1에서는 실제로 A/B가 성립합니다.

## 5. 확인됨 — `aot`와 `aot-dynamic`은 pumpit3에서 이미 동작하지 않습니다

같은 빌드로 세 backend를 직접 돌린 결과입니다.

| backend | pumpit3 | pumpit1 |
|---|---|---|
| `aot-dbt` | exit 0 | exit 0 |
| `aot-dynamic` | **exit 1** | exit 0 |
| `aot` | **exit 1** | 미측정 |

실패 메시지는 §4와 **동일**합니다. 원인이 같기 때문입니다 — 두 backend는
`enable_dbt_direct_edge_dispatch`가 거짓이므로 pumpit3 이미지를 생성할 수 없습니다.

**이 사실은 설계 §2의 전제를 정정합니다.** 설계는 "세 build option의 A/B 수단이
`aot-dynamic`뿐"이라고 적었으나, pumpit3에서는 `aot-dynamic` 자체가 기동하지 않으므로
그 축은 존재한 적이 없습니다. pumpit1에서만 성립했습니다.

결과적으로 Task 424는 잃어버린 축을 복원한 것이 아니라 **없던 축을 만든** 작업입니다.
현재 작업(Tasks 411~423)이 전부 pumpit3 대상이므로 실질적인 추가입니다. Task 425의
근거도 강해집니다 — `aot`와 `aot-dynamic`은 미사용일 뿐 아니라 주 대상에서 깨져 있는
설정이며, 제거는 오해를 부르는 구성을 없애는 일이 됩니다.

## 6. 남은 것

* 설계 §2와 §7을 §5의 실측으로 정정합니다.
* [frontier](../analysis/current-execution-frontier.md)에 §4·§5를 확인됨 항목으로 넣습니다.

---

# Task 424 Work Log — toggles for three DBT build options

Design: [20260805-424](../design/20260805-424-execution-backend-consolidation.md). Work order:
[20260805-424](../work-orders/20260805-424-dbt-build-option-toggles.md).

## 1. What was done

Added the `runtime::env_toggle` module (`ResolvePromotedToggle`, `ResolveOptInToggle`) and moved
the parsing convention that was inlined six times in `main.cpp` into it. Gave each of the three
backend-decided build options its own `REPIU_AOT_DBT_*` toggle, added a dedicated log line for
return-miss dispatch — which previously had none — and put the `enabled` value on the
direct-edge line. `env_toggle_probe` checks all seventeen rows of both truth tables.

## 2. One deviation from the work order

`ResolveAotDbtPostHleTranslationEnabled` in `aot_dbt_dispatch.cpp` was **not** converted, using
the exception §4 allows. It accepts the same value set, but it belongs to a family of eighteen
`Resolve*(std::string_view setting)` policy resolvers, each with its own probe — a different
layer from the getenv-parsing convention, and converting one alone would break step with its
siblings.

## 3. Verification

Release build exit 0; `repiu_aot_probe` exit 0 with `env_toggle_policy=true` and every existing
policy probe passing.

**Default equivalence against the pre-change commit `3909e82`**, built in a separate worktree
and run under identical conditions (one-second pumpit3, arena base `0x03000000` in both,
per-run isolated EEPROM): every logged value matched — inline-cache slots 4, guarded segment
pop/load/read `true`, `true/54`, `true/43`, direct-edge sites 10, superblock and
segment-override `false`, Port-I/O and Glide-gate-direct `true`, timer safe points `true/550` —
**including the runtime counters** segment-pop `36/4`, segment-load `8/5`, and generation
publishes/quarantines `0/0`. The only differences are the new return-miss line and the `enabled`
field on the direct-edge line, so the six pre-existing toggles kept their meaning.

Per-toggle OFF runs: return-miss reads `false` and execution continues with the other two
unmoved; timer safe points read `false/0` and execution continues; `=yes` also yields `false/0`,
confirming the fail-closed rule; direct-edge OFF exits 1, see §4. One `TIMER_SAFE_POINTS=0` run
reported eight direct-edge sites rather than ten, but that run alone landed at arena base
`0x07000000`; rerun at `0x03000000` it reported ten. That is the relocation-base pitfall Task
418's work order warns about, not a toggle effect.

## 4. Confirmed — direct-edge dispatch is mandatory for some images

`REPIU_AOT_DBT_DIRECT_EDGE_DISPATCH=0` exits before execution on pumpit3 with
`direct control-flow target is outside the cache`: that image has ten direct edges whose targets
lie outside the cache, and without the dispatch the emitter cannot represent them. pumpit1, with
none, builds and runs with the feature off. This is design §7's anticipated newly discovered
dependency rather than a regression, and the toggle stays — it is image-dependent rather than a
trap, it fails loudly with a message naming the cause, and on pumpit1 it does provide a real A/B.

## 5. Confirmed — `aot` and `aot-dynamic` no longer work on pumpit3

Running all three backends on the same build: `aot-dbt` exits 0 on both targets, while
`aot-dynamic` and `aot` exit 1 on pumpit3 with **the same message as §4** — both leave
`enable_dbt_direct_edge_dispatch` false and therefore cannot build that image. `aot-dynamic`
runs fine on pumpit1.

**This corrects design §2.** The design stated that `aot-dynamic` was the only way to A/B the
three options; on pumpit3 that axis never existed, because the backend does not start. It held
only for pumpit1. Task 424 therefore did not restore a lost axis — it **created one that was
never there**, on the target all current work (Tasks 411-423) uses. It also strengthens Task
425: `aot` and `aot-dynamic` are not merely unused but broken on the primary target, so removing
them deletes misleading configuration rather than dead code.

## 6. Remaining

Correct design §2 and §7 with the §5 measurements, and record §4 and §5 as confirmed items in
[frontier](../analysis/current-execution-frontier.md).
