# 20260724-283 작업 지시: AOT-DBT indirect host dispatch call/jump 분리 통제 실험

## 배경 (Korean)

Task 282는 indirect call/jump host dispatch(A안)를 구현했으나, 실제 `aot-dbt`
Glide 경로에서 결정적으로 크래시하여 기본 비활성(opt-in)으로 두었습니다. Task 282의
통제 실험은 layout 변경, inline-cache patch, FPU/SSE clobber를 근인에서 배제하고
근인을 **성공 전이(success transfer) 자체**로 좁혔습니다.

정적 재분석에서 다음이 확인됐습니다.

- host-dispatch의 스택/레지스터 산술은 call(`C3`)·jump(`C2 04 00`) 양쪽에서 증명상
  VEH `CONTINUE_EXECUTION` 경로와 동일합니다.
- **RET host-dispatch 성공 경로도 동일하게 `HandleAotReentry`와 메인 VEH dispatch를
  우회**하지만 라이브에서 성공 884회를 무크래시로 처리합니다(Task 281). 따라서
  "VEH 부수효과 누락"은 크래시의 충분조건이 아닙니다.
- 근인은 indirect success 전이가 RET과 **다르게** 하는 것에 있습니다. 가장 유력한
  미검증 용의자는 **CALL 경로의 guest-stack 반환주소 write**
  (`HandleAotIndirectTransfer`의 `WriteGuestUInt32([Esp-4], return_addr)` + `Esp -= 4`)
  입니다. RET·JUMP 경로에는 이 write가 없습니다.

Task 282의 실험은 call/jump를 분리하지 않았으므로, 본 작업은 call만/jump만 host
dispatch를 켜는 A/B 통제 실험으로 근인을 CALL 경로 여부로 이분합니다.

## Background (English)

Task 282 implemented indirect call/jump host dispatch (option A) but it crashes the live
`aot-dbt` Glide path, so it is opt-in and OFF by default. Its controlled experiments ruled
out the layout change, inline-cache patching, and FPU/SSE clobber, narrowing the cause to
the success transfer itself.

Static re-analysis established that the stack/register arithmetic is provably identical to
the VEH path for both call (`C3`) and jump (`C2 04 00`), and that the RET host-dispatch
success path bypasses the same `HandleAotReentry`/VEH side effects yet succeeds 884 times
live without crashing (Task 281). The remaining indirect-specific difference is the CALL
path's guest-stack return-address write (`WriteGuestUInt32([Esp-4], …)` + `Esp -= 4`),
absent from the RET and JUMP paths. Task 282 never split call vs jump, so this work adds a
calls-only / jumps-only A/B gate to bisect the cause by instruction kind.

## 변경 범위 (Scope)

1. `AotCodeCacheBuildOptions`에 `enable_dbt_indirect_dispatch_calls`,
   `enable_dbt_indirect_dispatch_jumps` (기본 `true`) 추가. master
   `enable_dbt_indirect_miss_dispatch`와 AND로 결합.
2. `EmitIndirectInlineCacheSlot`을 site 종류별로 게이트(call이면 calls 플래그, jump이면
   jumps 플래그). 기존 단일 bool 경로는 두 플래그가 모두 `true`일 때와 바이트 동일.
3. `REPIU_AOT_DBT_INDIRECT` 파싱 확장: `1`/`both`=양쪽, `call`/`calls`=call만,
   `jump`/`jumps`=jump만, 그 외/미설정=off. 기본 동작(off) 불변.

## 검증 (Verification)

- VS Win32 x86 Debug 빌드, `repiu_aot_probe` 전체 통과(특히 `dbt_indirect_dispatch_all`이
  master 플래그만으로 여전히 call/jump 양쪽 layout을 방출).
- 격리 EEPROM `aot-dbt` 헤드리스 3회: `REPIU_AOT_DBT_INDIRECT=call`,
  `=jump`, 대조군 `=0`. 각 실행의 크래시 여부·progress·indirect 회계·EEPROM SHA-256 기록.
- 결과를 `docs/analysis/current-execution-frontier.md`와 Task 283 작업 로그에 반영.

## 비고

이 작업은 Task 282의 opt-in·비활성 상태를 바꾸지 않는 **진단 계측**입니다. 기본
`aot-dbt`(indirect off) 동작은 그대로 유지됩니다.
