# 20260723-277 작업 로그: AOT-DBT return miss host dispatch

## 한국어

### 구현

- `AotCodeCacheBuildOptions`에 DBT return miss policy를 추가하고, 기존 backend의
  return byte layout은 그대로 유지했습니다.
- 플랫폼 공용 emitter가 DBT return site의 guest source, miss immediate, thunk rel32,
  fallback/success continuation offset을 기록합니다.
- Win32 static placement와 dynamic append가 기존 RW 구간에서 절대 miss 주소와 host
  thunk를 연결하고 metadata offset을 재배치합니다.
- 전용 `aot_dbt_return_dispatch.*`가 guest register/EFLAGS를 저장하고 host ESP/TEB
  stack bounds로 전환한 뒤 기존 return handler와 worker patch protocol을 재사용합니다.
- 성공은 `ret imm16` continuation으로 원본 stack 의미를 보존하고, 실패는 DBT metadata만
  제거한 뒤 기존 provenance `INT3`/VEH로 fail-closed합니다.
- 시도/성공/fallback 카운터와 실행 종료 요약을 추가했습니다.

### 수정 과정

첫 실제 실행은 `39,296/0/39,296`으로 전부 fallback했습니다. 기존 handler의
`aot_reentry_pending` 계약을 normal-call adapter가 설정하지 않은 것이 원인이었고,
호출 전 상태를 맞춘 뒤 성공 경로가 활성화됐습니다. fallback 설계 덕분에 이 과정에서
guest 의미 손상, fatal 또는 EEPROM 변경은 없었습니다.

VS 2026 x86 산출물은 guest 실행 전에 저주소 공간을 광범위하게 점유해 relocation
후보가 모두 막혔습니다. 프로젝트 검증 기준인 VS 2022 Win32를 별도 build directory에
구성해 실제 실행을 검증했습니다. 동일 VS 2022 산출물도 프로세스별 DLL 배치에 따라
일부 시작 시도가 relocation 전에 종료될 수 있었으며, 정상 배치된 실행만 비교했습니다.

### 검증

- VS 2026 Win32 x86 Debug 전체 compile/link 성공(실행은 위 주소 배치 문제로 제외)
- VS 2022 Win32 x86 Debug compile/link 성공
- AOT synthetic probe 전체 통과
  - backend policy
  - legacy return layout
  - DBT return layout/placement
  - indirect inline-cache chain/round-robin/retirement
  - native linear span
  - SMC coherence
- 최종 15초 `aot-dbt`:
  - exception caught false, graceful timeout
  - DBT return attempt/success/fallback `5,507/849/4,658`
  - DBT HLE reentry `8,390/2,357`
  - AOT boundary/re-entry `17,662/18,546`
  - progress `13,251`
  - fatal 0, legacy fallback 0
- 대조 15초 `aot-dynamic`:
  - DBT return `0/0/0`
  - AOT boundary/re-entry `17,781/17,816`
  - progress `12,745`
  - fatal 0, legacy fallback 0
- 두 EEPROM SHA-256 모두 원본과 일치

### 결과

`aot-dbt` return miss 중 확인된 849회의 Windows breakpoint exception 왕복을 제거했고,
해결할 수 없는 4,658회는 기존 정확성 경로를 보존했습니다. 단일 timing 표본의 progress
차이는 성능 향상으로 주장하지 않습니다. 다음 후보는 fallback 사유 계측 또는 같은
host-stack ABI를 indirect call/jump miss로 확장하는 작업입니다.

## English

Implemented a DBT-only return-miss layout and Win32 placement metadata while
leaving existing backend bytes unchanged. The dedicated naked x86 thunk saves
guest registers/EFLAGS, switches to the saved host ESP and TEB stack bounds, and
reuses the established return resolver and serialized patch worker. Success uses
`ret imm16`; failure removes only DBT metadata and reaches the existing
provenance-aware `INT3`/VEH path.

The first live run safely produced 39,296/0/39,296 attempts/successes/fallbacks
because the normal-call adapter omitted the existing `aot_reentry_pending` entry
contract. Establishing that state enabled the success path without any prior
guest corruption, fatal state, or EEPROM mutation.

VS 2022 Win32 x86 Debug and all backend/layout/inline-cache/native-span/SMC probes
passed. The final 15-second `aot-dbt` run recorded 5,507/849/4,658 return
attempts/successes/fallbacks, 17,662/18,546 boundaries/re-entries, progress
13,251, zero fatal state, and zero legacy fallback. The `aot-dynamic` control
recorded 0/0/0, 17,781/17,816, progress 12,745, zero fatal state, and zero legacy
fallback. Both isolated EEPROM hashes matched the original. The timing samples
are not treated as a performance claim; 849 avoided breakpoint/VEH round trips
are the confirmed local effect.
