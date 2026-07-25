# 20260725-291 guarded segment-pop fast path 작업 지시서 / Guarded segment-pop fast-path work order

## 한국어

### 목표

`POP ES/DS/FS/GS`가 현재 물리 selector, shadow selector, guest stack selector를 바꾸지
않는 경우에만 AOT cache 안에서 처리하여 planner HLE/TF single-step 예외 왕복을
줄입니다.

### 작업 범위

- [x] planner에 plain segment-pop 전용 record와 보수적 opcode 분류를 추가합니다.
- [x] EAX/EFLAGS 보존, physical/stack/shadow 3자 일치 guard, `ESP += 4`, cache
  fallthrough, 원상복구 `INT3` fallback을 방출합니다.
- [x] Win32 정적 배치와 dynamic append에서 shadow/counter 주소를 patch합니다.
- [x] placement metadata와 cache breakpoint provenance index를 갱신합니다.
- [x] whole-CFG HLE coverage validator에 guarded segment-pop 구조 검증을 추가합니다.
- [x] synthetic probe로 지원·거부 opcode, layout, patch, fallback을 검증합니다.
- [x] 환경 정책과 success/fallback 계측을 추가합니다.
- [x] Win32 x86 Debug 전체 빌드와 관련 probe를 실행합니다.
- [x] 동일 binary·격리 EEPROM 교차 A/B를 수행하고 기본 정책을 결정합니다.
- [x] `ARCHITECTURE.md`, 관련 analysis, 설계서와 작업 로그에 결과를 반영합니다.

### 범위 밖

- selector 값이 실제로 바뀌는 segment load의 exception-free emulation
- `POP SS`, operand/address-size prefix segment pop
- `MOV Sreg,r/m`, `MOV r/m,Sreg` 의미 변경
- selector descriptor, low-memory, quarantine/SMC 정책 변경

### 완료 조건

- fallback 진입 시 guest register/flags/ESP가 원본 segment-pop 진입 상태와 같습니다.
- 성공은 `physical == stack == shadow`일 때만 발생합니다.
- build와 기존 AOT/DBT/selector/coherence probe가 통과합니다.
- fatal/legacy fallback은 0이고 EEPROM hash는 대조군과 일치하며 late Glide milestone은
  유지되거나 개선됩니다.
- success 모집단과 성능 효과를 수치로 기록하고 default ON 또는 opt-in 유지 결론을
  남깁니다.

## English

### Goal and scope

Handle plain `POP ES/DS/FS/GS` inside the AOT cache only when the physical selector, shadow
selector, and guest-stack selector are already identical, reducing planner-HLE/TF
single-step exception round trips.

Add a dedicated planner record, an EAX/EFLAGS-preserving three-way guard, success stack
advance and cache fallthrough, an exact-state INT3 fallback, static/dynamic shadow/counter-address
patching, placement metadata, provenance indexing, whole-CFG structural validation,
synthetic probes, an environment policy, and success/fallback counters. Run the full Win32
x86 Debug
build and an alternating same-binary isolated-EEPROM A/B, then update architecture, analysis,
design, and work log with the measured policy decision.

General selector-changing load emulation, `POP SS`, prefixed forms, changes to `MOV Sreg`
semantics, and selector/low-memory/quarantine/SMC policy are out of scope. Completion requires
exact fallback state, success only under the three-way equality predicate, all relevant probes
passing, zero fatal/legacy fallback, matching EEPROM, non-regressed late milestones, and a numeric
promotion or opt-in conclusion.
