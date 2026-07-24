# 20260724-288 작업 지시: 네이티브 직선 span 확장 / Work order: native linear-span extension

설계: [docs/design/20260724-288-native-linear-span-extension.md](../design/20260724-288-native-linear-span-extension.md)

## 한국어

### 목표

`aot-dbt` fallback의 native linear span이 진입당 실행하는 명령 수를 늘리고 진입 비용을
낮춰 fail-closed 구간의 single-step을 추가로 줄인다. 게스트 실행 의미, quarantine/SMC
정책, AOT layout은 바꾸지 않는다.

### 범위

- 대상: `ScanNativeLinearSpanWithZydis`
  ([verified_region_analyzer.cpp](../../src/platform/win32/verified_region_analyzer.cpp)),
  `native_linear_span.{h,cpp}`, `NativeFastPathState`
  ([native_fast_path.h](../../src/platform/win32/native_fast_path.h)),
  page coherence generation/write-watch
  ([aot_page_coherence_win32.cpp](../../src/platform/win32/aot_page_coherence_win32.cpp)),
  `native_linear_span_probe.{h,cpp}`, 벤치 스크립트 2종.
- 비대상: 전이 경계 dispatch(Task 276~286), planner/emitter 번역 정책(Task 289), 다른
  backend 기본값.

### 작업 단계 (각 단계는 독립 커밋과 독립 A/B)

세 단계는 위험 대비 가치 순서다. 앞 단계 통과 없이 다음 단계를 켜지 않는다.

- [x] **Stage 1 — page-generation decode 캐시 (구현 완료, 기본 활성화 보류)**
  - [x] `NativeFastPathState`에 span 스캔 캐시(entry EIP → 결과)와 캐시 hit/miss 카운터 추가.
  - [x] coherence 계층의 페이지 generation을 조회하는 접근자를 확인/추가하고, span 코드
        페이지 generation을 캐시 키에 포함.
  - [x] write-watch/generation bump 시 해당 페이지 캐시 항목 무효화.
  - [x] coherence 미추적 페이지는 캐시하지 않고 재스캔(fail-safe).
  - [x] probe: generation 무효화 후 stale 캐시 미사용 검증.
  - [x] 예비 A/B: hit 0을 확인해 240초 3쌍 전에 조기 보류. 수치는 작업 로그에 기록.

- [x] **Stage 2 — non-aliasing memory write 통과 (구현 완료, 기본 활성화 보류)**
  - [x] 선결: AOT-backing 게스트 페이지의 write-guard(write-watch/read-only) 실제 커버리지
        조사 결과를 작업 로그에 기록.
  - [x] 스캐너에 "span 코드 페이지 전체가 write-guard로 덮임" 술어 추가.
  - [x] 술어 참일 때만 `HasExplicitMemoryWrite`를 경계로 삼지 않고 통과. 거짓이면 기존
        경계 유지(fail-closed).
  - [x] write-cross / guard-uncovered 경계 / write-fault 정상 중단 카운터 추가.
  - [x] probe: guard 커버 페이지에서만 write 통과, 미커버·entry write·변경된 주소
        register·부적합 target은 경계 유지.
  - [x] 예비 A/B: 240초 direct 의미 milestone 회귀로 3쌍 전에 조기 보류.

- [x] **Stage 3 — forward direct `jmp` 체인 (구현 완료, 기본 활성화 보류)**
  - [x] 스캐너가 in-range·non-boundary·forward target의 direct unconditional `jmp rel`을
        만나면 종료 대신 target에서 스캔 계속. backward target은 종료(loop 방지).
  - [x] indirect/far/HLE-boundary/quarantine target은 통과하지 않음.
  - [x] branch-chain / backward-stop 카운터 추가.
  - [x] probe: forward jmp 체인, backward 종료, HLE와 동등한 거부 target 종료.
  - [x] (2차 후보) Dr1 conditional branch 감시는 실제 forward chain 0으로 진행하지 않음.
  - [x] A/B: 10초 smoke와 60초 supervisor에서 chain 0을 확인해 장기 측정 조기 종료.

### 정확성 게이트 (모든 단계 공통)

- [x] Stage 1 ON 실행에서 `span_entry == span_boundary`, `span_cancel == 0`.
- [x] Stage 1 fatal 0, legacy fallback 0.
- [x] Stage 1 EEPROM SHA-256이 원본과 일치.
- [x] Stage 1 의미 기반 Glide milestone이 OFF와 모순 없음.
- [x] Stage 2 60초 ON 실행에서
      `span_entry == span_boundary + span_write_fault_cancel`, unexpected cancel 0.
- [ ] Stage 2 240초 direct에서는 OFF/ON 공통 late single-step cancel이 발생해 zero-cancel
      게이트를 만족하지 못함. 기능은 기본 활성화하지 않음.
- [x] Stage 3 60초 ON 실행에서 `span_entry == span_boundary`, `span_cancel == 0`.
- [x] Stage 3 fatal 0, legacy fallback 0, EEPROM SHA-256 일치.

### 검증 절차

- [x] Stage 1/2/3 VS2022 Win32 x86 Debug 전체 빌드 성공.
- [x] Stage 1/2/3 `repiu_aot_probe` 전체 통과(기존 + 신규 스캐너 케이스).
- [x] 각 Stage OFF/ON 교차 A/B. 가치 0 또는 명확한 회귀 시 계획대로 장기 측정 조기 종료.
- [x] 각 Stage 작업 로그에 수치와 승격/보류 판정 기록.

### 산출물

- [x] 코드 변경(단계별 커밋; Stage 1/2/3 완료).
- [x] `native_linear_span_probe` Stage 1/2/3 확장.
- [x] 작업 로그 `docs/work-logs/20260724-288-native-linear-span-extension.md`.
- [x] Stage 1/2/3 결과를 `docs/analysis/`와 `ARCHITECTURE.md`에 갱신.

## English

### Goal

Increase instructions executed per `aot-dbt` native linear span and lower per-entry scan cost
to further reduce fail-closed single-stepping, without changing guest semantics,
quarantine/SMC policy, or AOT layout.

### Scope

In: `ScanNativeLinearSpanWithZydis`, `native_linear_span.{h,cpp}`, `NativeFastPathState`, page
coherence generation/write-watch, the native-span probe, and the two benchmark scripts. Out:
transfer-boundary dispatch (Tasks 276-286), planner/emitter translation policy (Task 289),
other backends' defaults.

### Ordered stages (independent commit and A/B each)

- **Stage 1 — page-generation decode cache.** Add a per-entry scan cache with hit/miss
  counters to `NativeFastPathState`, keyed with the code-page generation from the coherence
  layer; invalidate on write-watch/generation bump; never cache untracked pages. Probe: no
  stale cache after invalidation. A/B: single-step, progress, cache hit rate, span lifecycle.
- **Stage 2 — cross non-aliasing memory writes.** First record actual write-guard coverage of
  AOT-backing pages in the log. Add a "span code pages fully write-guarded" predicate; pass
  writes only when true, else keep the boundary. Add write-cross / uncovered counters. Probe:
  cross only on covered pages. A/B: mean span length, single-step, progress, cancel rate,
  milestones.
- **Stage 3 — chain forward direct `jmp`.** On an in-range, non-boundary, forward direct
  unconditional `jmp rel`, continue scanning at the target instead of stopping; stop on
  backward targets (loop guard); never pass indirect/far/HLE/quarantine targets. Add
  branch-chain / backward-stop counters. Probe: forward chain, backward stop, HLE stop.
  Second candidate: conditional branches via Dr1, decided separately. A/B: single-step,
  progress, chain count, milestones.

### Correctness gate (all stages)

Stages 1 and 3 keep `span_entry == span_boundary` with zero cancellation. Stage 2 keeps
`span_entry == span_boundary + span_write_fault_cancel` with zero unexpected cancellation.
Every ON run keeps zero fatal/legacy fallback, matching EEPROM SHA-256, and semantic
milestones consistent with OFF.

### Verification

Full VS2022 Win32 x86 Debug build; all `repiu_aot_probe` cases (existing + new scanner cases);
three alternating supervisor A/B pairs per stage, supplemented by three direct-loader pairs
when late milestones are censored; per-stage log with numbers and promote/hold decision.

Stage 1 is implemented and verified but held default-off. One valid 60-second supervisor
pair and one valid direct-loader pair both recorded zero cache hits, so the planned
three-pair 240-second campaign was stopped early as incapable of demonstrating decode
reuse. The build, generation-invalidation probe, coherence probes, span lifecycle gates,
fatal/legacy fallback gates, and EEPROM hash gate all passed. Later stages are recorded below.

Stage 2 is implemented and verified but also held default-off. It requires every traversed
code page to be write-watched, leaves entry writes and writes with modified address registers
at the old boundary, and preflights the effective target through a cached page-protection
query. The final 60-second supervisor pair satisfied
`entry == boundary + write_fault_cancel` with zero unexpected cancellation. A 240-second
direct pair nevertheless reduced draw/swap from `1,606/233` to `1,278/185`; both conditions
also recorded comparable late single-step cancellations. The three-pair campaign was stopped
early because the semantic milestones clearly regressed. Stage 3 is recorded below.

Stage 3 is implemented and verified but held default-off. It chains only in-range forward
near direct jumps whose targets are neither HLE boundaries nor quarantined pages; backward,
indirect, far, and rejected targets retain the old boundary. Synthetic forward-chain,
disabled, backward-stop, and rejected-target probes pass. A 10-second smoke and a valid
60-second supervisor pair both recorded zero forward chains; the enabled 60-second run
recorded 703 backward stops, exact entry/boundary equality, zero cancellation/fatal/legacy
fallback, and a matching EEPROM hash. With no observed optimization opportunity, the
longer campaign and conditional-branch Dr1 candidate were stopped early.

### Deliverables

Staged code commits; extended `native_linear_span_probe`; work log
`docs/work-logs/20260724-288-native-linear-span-extension.md`; span analysis docs and
`ARCHITECTURE.md` updated as needed.
