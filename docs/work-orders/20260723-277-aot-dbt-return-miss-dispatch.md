# 20260723-277 작업 지시: AOT-DBT return miss host dispatch

## 한국어

### 목표

`aot-dbt`의 translated return inline-cache miss를 정상 host-stack dispatcher로
처리해 `INT3`/VEH 왕복을 제거하고, 실패 시 기존 return dispatcher로 정확히
복귀시킵니다.

### 구현 범위

1. platform-neutral DBT return site metadata와 build option을 추가합니다.
2. `C3`/`C2 iw` DBT miss tail과 success/fallback continuation을 방출합니다.
3. Win32 placement와 dynamic append에서 cache 절대 주소와 host thunk를 연결합니다.
4. guest register/EFLAGS와 TEB stack bounds를 보존하는 Win32 x86 host-stack thunk를
   전용 AOT-DBT 파일에 구현합니다.
5. 기존 return target resolution, telemetry와 serialized inline-cache patch 정책을
   공용 helper로 재사용합니다.
6. synthetic probe, Win32 x86 Debug 빌드, 기존 AOT/SMC probe와 실제 비교 실행을
   검증합니다.
7. 아키텍처·분석·작업 로그를 갱신하고 하나의 작업 커밋으로 남깁니다.

### 비범위

- indirect call/jump miss의 host dispatch
- HLE callback의 일반 host-call ABI
- 미번역 target의 새 interpreter 또는 별도 IR
- 기존 backend의 layout/동작 변경
- multi-thread code-cache publication

### 완료 기준

- DBT off return slot은 기존 byte layout을 유지
- DBT on `C3`/`C2` miss가 성공 시 VEH 없이 cache target으로 이동
- 모든 실패가 기존 `INT3` return dispatcher로 fail-closed
- register, EFLAGS와 guest ESP 의미가 probe 및 실제 실행에서 보존
- 빌드/probe 통과, fatal/새 legacy fallback/EEPROM 변경 없음

## English

### Goal

Route translated return inline-cache misses through a normal host-stack
dispatcher under `aot-dbt`, eliminating the `INT3`/VEH round trip while
preserving the existing dispatcher as the fail-closed fallback.

### Scope

Add platform-neutral DBT return-site metadata and policy, emit `C3`/`C2`
success/fallback continuations, resolve them during Win32 placement and dynamic
append, implement a dedicated x86 host-stack thunk, reuse the established return
resolver/telemetry/worker patch path, and verify synthetic plus live execution.

Indirect call/jump dispatch, a general HLE callback ABI, a new interpreter/IR,
changes to existing backends, and multi-thread publication are out of scope.

Completion requires byte-for-byte legacy layout stability when disabled,
exception-free successful DBT misses, exact fallback behavior, preserved
register/flags/ESP semantics, passing builds/probes, and no fatal state, new
legacy fallback, or EEPROM mutation.
