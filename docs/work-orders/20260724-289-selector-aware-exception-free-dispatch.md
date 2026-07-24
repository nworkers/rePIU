# 20260724-289 작업 지시: selector 인지 exception-free dispatch / Work order: selector-aware exception-free dispatch

설계: [docs/design/20260724-289-selector-aware-exception-free-dispatch.md](../design/20260724-289-selector-aware-exception-free-dispatch.md)

## 한국어

### 목표

hot phase 최대 경계 범주 `other`(70,957회, 일반 HLE boundary·미번역 fallthrough·arbitrary
cache miss)를 exception-free host dispatch로 전환한다. 선결 조건은 emitter에 live
segment/selector 정책을 공급해 selector 0 저메모리와 shadow selector 발산 두 회귀를 막는
것이다.

### 범위

- 대상: `IsHleBoundary`, `IsTranslatableSegmentOverrideMem`, `kSegmentOverrideMem` emit 경로
  ([aot_translation_plan.cpp](../../src/runtime/aot_translation_plan.cpp),
  `aot_code_cache.cpp`), post-HLE 재진입 정책
  ([execution_trampoline.cpp](../../src/platform/win32/execution/execution_trampoline.cpp),
  [aot-dbt-post-hle-reentry.md](../analysis/aot-dbt-post-hle-reentry.md)), aot-dbt host-stack
  ABI(Task 277/282), 관련 aot_probe.
- 비대상: native span 확장(Task 288), quarantine/SMC 정책 완화, 다른 backend layout.

### 선결

- [x] 게스트 **shadow selector** 상태를 emitter/런타임에 노출하는 최종 구조 확정:
      Task 264의 `guest_*` shadow 주소/값 + `selector_table` + segment site patch ABI 재사용.
      ([aot-dbt-post-hle-reentry.md](../analysis/aot-dbt-post-hle-reentry.md) 미확정 3항목).
- [ ] Task 264 Phase 2 revert와 Task 276 post-HLE 크래시를 회귀 감시 기준 케이스로 고정.

### 작업 단계 (기반 우선, 각 단계 opt-in→증거 후 승격)

- [x] **Stage 1 — live selector descriptor 모델 + 런타임 guard (완료)**
  - [x] 기존 resolution을 descriptor 존재/base/limit/flags/native-fold 상태로 확장.
  - [x] selector 0·DOS low-memory descriptor는 boundary, 정상 flat 및 검증된 GS base-add는
        guarded native로 분류.
  - [x] selector뿐 아니라 descriptor 전체 fingerprint 변경 때 site 재해석.
  - [x] segment-override memory 접근을 guarded native로 emit: shadow selector가 기대
        descriptor면 folded native, 아니면 HLE low-memory 핸들러로 exit.
  - [x] emit 시 검증 불가 형태는 기존 boundary로 fail-closed 유지.
  - [x] native/HLE/unresolved 활성 site 및 실제 HLE-exit/mismatch 카운터 추가.
  - [x] probe: shadow flat → folded native, shadow selector 0 → HLE-exit, mismatch →
        fail-closed.
  - [x] 60초 기본 smoke: progress 48,817, fatal/legacy fallback 0, EEPROM 일치.

- [x] **Stage 2 — post-HLE cache miss 재번역 + 전체 CFG HLE-boundary 보장 (보류)**
  - [x] 생성 CFG의 모든 명령이 HLE boundary이거나 Stage 1 guard를 갖는지 검증하는 whole-CFG
        preflight 추가.
  - [x] 통과 시 HLE 직후 cache miss를 single-step 대신 즉시 번역·복귀하는 opt-in 추가.
  - [x] 실패 시 기존 segment-write barrier/직선 preflight TF bridge 유지(fail-closed).
  - [x] probe: 정상 CFG 통과, guard 누락 CFG 거부, selector 0은 Stage 1 HLE 경계.
  - [x] 10초/60초 A/B: 번역 시도 0으로 장기 측정 조기 종료, 기본 OFF 유지.

- [x] **Stage 3 — arbitrary miss / 미번역 fallthrough host dispatch (모집단 0, 보류)**
  - [x] cache breakpoint provenance를 먼저 분리해 범용 dispatch 전제를 검증.
  - [x] 60초 census에서 unknown/arbitrary 후보 0 확인.
  - [x] planner HLE·selector guard·inline fallback·retired entry는 기존 전용 경로 유지.
  - [x] 후보가 없으므로 위험한 범용 host tail은 추가하지 않고 보류.

### 정확성 게이트 (모든 단계 공통)

- [x] 두 revert 선례(selector 0 저메모리, shadow selector 발산) 무회귀.
- [x] fatal 0, legacy fallback 0, caught exception false.
- [x] EEPROM SHA-256 원본 일치.
- [x] 의미 기반 Glide milestone이 control 경로와 일치.
- [x] 증명 불가 지점은 전부 기존 `INT3`/VEH single-step으로 fail-closed.

### 검증 절차

- [x] VS2022 Win32 x86 Debug 전체 빌드 성공.
- [x] `repiu_aot_probe` 전체 통과(기존 + selector-guard + whole-CFG preflight + provenance).
- [x] 동일 binary·격리 EEPROM·교차 순서 A/B(Backend=`aot-dbt`), supervisor 검열 시 직접
      loader 보완.
- [x] 각 Stage 작업 로그에 수치와 승격/보류 판정 기록.

### 산출물

- [x] 코드 변경(단계별 커밋).
- [x] selector-guard / whole-CFG preflight / breakpoint provenance probe.
- [x] 작업 로그 `docs/work-logs/20260724-289-selector-aware-exception-free-dispatch.md`.
- [x] `docs/analysis/aot-dbt-post-hle-reentry.md` 미확정 항목 갱신, `ARCHITECTURE.md` 및
      `docs/EXE_DESIGN.*` selector/shadow 관련 갱신.

## English

### Goal

Convert the largest hot-phase boundary category — `other` at 70,957 (general HLE boundaries,
untranslated fallthrough, arbitrary cache misses) — to exception-free host dispatch. The
prerequisite is feeding the emitter a live segment/selector policy that prevents the two known
regressions: selector-0 low-memory misfold and shadow-selector divergence.

### Scope

In: `IsHleBoundary`, `IsTranslatableSegmentOverrideMem`, the `kSegmentOverrideMem` emit path,
the post-HLE re-entry policy, the aot-dbt host-stack ABI (Task 277/282), and related probes.
Out: native-span extension (Task 288), quarantine/SMC relaxation, other backends' layouts.

### Prerequisite

Fix the structure exposing guest **shadow selector** state to the emitter/runtime (the three
open items in `aot-dbt-post-hle-reentry.md`), and pin the Task 264 Phase 2 revert and the Task
276 post-HLE crash as regression-watch cases.

### Ordered stages (foundation first; opt-in until evidence promotes)

- **Stage 1 — live selector descriptor model + runtime guard.** Define a
  selector→descriptor (base/limit/flat) policy; emit segment-override accesses as guarded
  native (folded native when the shadow selector maps to the expected flat descriptor, else HLE
  low-memory exit); keep unverifiable forms as boundaries (fail-closed). Add fast-path
  hit/HLE-exit/mismatch counters. Probe: shadow flat→native, shadow selector-0→HLE-exit,
  mismatch→fail-closed. A/B: no regression on both revert cases, single-step, progress, hash.
- **Stage 2 — post-HLE re-translation with whole-CFG HLE-boundary guarantee.** Add a whole-CFG
  preflight verifying every instruction is an HLE boundary or carries the Stage 1 guard; on
  pass, translate and re-enter the post-HLE miss instead of single-stepping; else keep the TF
  bridge. Probe: reject CFGs missing a guard/boundary; the Task 276 crash address does not
  reproduce. A/B: single-step, progress, post-HLE re-entry rate, texture/draw/swap.
- **Stage 3 — host dispatch for arbitrary miss / untranslated fallthrough.** Route the
  remaining `other` boundaries through the Task 277/282 host-stack ABI; fail closed for
  indirect unknown targets, quarantined SMC pages, or non-flat selector reports; account
  attempts/successes/causes reusing the Task 281 classification. Probe: dispatch success and
  each fail-closed cause. A/B: `other`-boundary reduction, single-step, progress, dispatch
  accounting, milestones.

### Correctness gate (all stages)

No regression on the two revert precedents; zero fatal/legacy fallback and no caught
exception; matching EEPROM SHA-256; semantic Glide milestones consistent with the control
path; every unprovable site fails closed to `INT3`/VEH single-step.

### Verification

Full VS2022 Win32 x86 Debug build; all `repiu_aot_probe` cases (existing + selector-guard +
whole-CFG preflight); same-binary isolated-EEPROM alternating A/B (Backend=`aot-dbt`),
supplemented by direct loader when supervisor milestones are censored; per-stage log with
numbers and promote/hold decision.

### Deliverables

Staged code commits; selector-guard / whole-CFG preflight probes; work log
`docs/work-logs/20260724-289-selector-aware-exception-free-dispatch.md`; updates to
`aot-dbt-post-hle-reentry.md` open items, `ARCHITECTURE.md`, and `docs/EXE_DESIGN.*`
selector/shadow notes.

## 한국어 — Stage 3 보정 작업

- [x] **Stage 3a — cache breakpoint provenance census**
  - [x] placement metadata로 planner HLE, selector guard, inline miss, jump-table fallback,
        retired/inactive entry, probe sentinel, unknown을 구조적으로 분류한다.
  - [x] 합성 probe에서 각 metadata 경계와 unknown의 fail-closed 분류를 검증한다.
  - [x] live/종료 telemetry에 원인별 카운터를 노출하고 `other` 합계와 대조한다.
  - [x] 동일 binary 60초 smoke로 host-dispatch 가능한 arbitrary miss/fallthrough 모집단의
        존재 여부를 판정한다.
- [x] **Stage 3b — 증명된 모집단만 host dispatch (보류 결정)**
  - [x] Stage 3a에서 모집단이 0이면 코드를 추가하지 않고 보류한다.
  - [ ] 모집단이 있으면 guest target, stack continuation, selector/HLE exclusion을 모두
        증명한 site metadata와 opt-in tail을 추가한다.

근거: completed image의 direct/conditional/block-fallthrough fixup은 내부 target으로
해소되지 않으면 build 자체가 실패합니다. 따라서 guest opcode 기반 `other` 분류를 곧바로
범용 host dispatch 후보로 간주하지 않습니다.

## English — corrected Stage 3 work

- [x] **Stage 3a — cache-breakpoint provenance census.** Structurally classify planner HLE,
  selector guard, inline miss, jump-table fallback, retired/inactive entry, probe sentinel,
  and unknown; add synthetic probes and live/final accounting; run a same-binary 60-second
  smoke to determine whether an arbitrary-miss/fallthrough population actually exists.
- [x] **Stage 3b — dispatch only a proven population (held).** Stage 3a found zero candidates,
  hold without adding a risky generic dispatcher. Otherwise require explicit site metadata
  proving the guest target, stack continuation, and selector/HLE exclusion before adding an
  opt-in host tail.

Rationale: a completed image cannot retain an unresolved direct, conditional, or block-
fallthrough fixup; image construction fails instead. Guest-opcode `other` is therefore not
itself proof of a generic host-dispatch candidate.
