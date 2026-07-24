# 20260724-282 작업 로그: AOT-DBT indirect call/jump miss host dispatch (A안)

## 한국어

### 구현

- 공용 image에 `AotDbtIndirectDispatchSite`와 `enable_dbt_indirect_miss_dispatch`
  옵션을 추가하고, `EmitIndirectInlineCacheSlot`이 옵션에 따라 3슬롯 miss tail
  (push return-addr/miss/guest-source), fallback continuation(`lea esp,[esp+8]; INT3`),
  call/jump별 success continuation(`C3` / `C2 04 00`)을 방출하게 했습니다. 옵션이 꺼지면
  기존 `popfd; INT3` 바이트를 그대로 유지합니다.
- Win32 static placement와 dynamic append가 miss 절대 주소와 thunk `rel32`를 해결하고
  append offset을 재배치하도록 `ResolveWin32AotDbtIndirectDispatchSites`와 site 복사
  경로를 추가했습니다.
- 전용 `aot_dbt_indirect_dispatch.{h,cpp}`에 naked thunk와 adapter를 추가했습니다.
  adapter는 site 존재·`guest_source` 일치·guest `FF /digit` 종류 일치를 검증한 뒤 저장된
  guest `CONTEXT`로 기존 `HandleAotIndirectTransfer`를 재사용합니다(A안). thunk는 guest
  register/EFLAGS 저장, host ESP/TEB 전환, FPU/SSE FXSAVE/FXRSTOR 후 resolver를 호출하고
  성공/실패 continuation으로 복귀합니다.
- Task 281 fallback 원인 enum을 두 경로 공용 `AotDbtDispatchFallbackReason`으로
  일반화(slot 3 = `kUnreadableSource`)하고, `ThreadContext`·공개 실행 결과·종료 로그에
  indirect 경로의 entry/attempt/success/fallback과 원인 벡터를 추가했습니다.
- **Task 281 attempt 회계 보정:** raw C++ 진입 수를 `entry`로 유지하고, 보고 attempt는
  snapshot에서 `success + fallback`으로 도출하도록 return·indirect 양쪽을 수정했습니다.
- synthetic probe `RunAotDbtIndirectDispatchProbe`를 추가했습니다.

### 검증

- VS2022/VS2026 Win32 x86 Debug 전체 빌드 성공.
- `repiu_aot_probe` 전체 통과, 신규 `dbt_indirect_dispatch_all=true`
  (call/jump layout, 비활성 시 기존 layout, placement miss-immediate/thunk rel32,
  fallback 원인 slot·회계 불변식).
- 기본 `aot-dbt`(indirect off) 30초 실구동: exception caught false, indirect `0/0/0/0`,
  return `entry=attempt=7141`·`success+fallback=940+6201=7141`, indir boundary(VEH) 8,891,
  progress 29,782, fatal 0, 격리 EEPROM SHA-256 불변. Task 281 상태 유지 확인.

### 라이브 크래시 조사

indirect dispatch를 켜면 Glide 초기화 중 결정적으로 access violation(0xC0000005, Glide
DLL 0x101xxxxx, EAX=EDX=0xEF6ADDDD)으로 크래시했습니다. 통제 실험:

- indirect OFF: 무크래시(progress 29,798).
- force-fallback(큰 layout + 모든 indirect를 INT3/VEH로): 무크래시(progress 26,355)
  → layout 변경은 근인 아님.
- no-patch(inline-cache patch 억제 + host transfer): 여전히 크래시 → patch 아님.
- FXSAVE/FXRSTOR: 크래시 불변 → FPU/SSE clobber 아님.
- 첫 30회 dispatch의 `src/op/esp/call` 시퀀스가 ON/OFF 완전 일치, 크래시는 30번째 직후
  CS-prefixed jump table에서 발생 → 손상은 정수 상태에 안 드러나고 누적됨.

성공 전이의 최종 레지스터/스택 상태는 VEH `CONTINUE_EXECUTION` 경로와 증명상 동일하므로,
남은 가설은 host-dispatch가 메인 VEH dispatch를 우회하며 놓치는 부수효과입니다. run마다
heap base가 달라 두 독립 실행의 포인터 교차 비교로는 분기점을 격리하지 못했습니다.

### 결과와 결정

기능은 구현·probe 검증을 마쳤으나 실행 정확성이 확인되지 않아 **기본 비활성(opt-in,
`REPIU_AOT_DBT_INDIRECT=1`)** 으로 두었습니다. attempt 회계 보정과 fallback 원인 계측은
독립적으로 견고하여 유지합니다. 다음은 단일 실행 결정적 관측(고정 base 또는 trap 백엔드
단일스텝)으로 부수효과 분기점을 격리하는 것입니다.

## English

Implemented Task 280 Stage 4 (option A): a platform-neutral `AotDbtIndirectDispatchSite`
and build option, an emitter that produces the three-slot host-dispatch miss tail with
per-kind continuations (or keeps the legacy `popfd; INT3` when disabled), Win32
placement/dynamic-append resolution, a dedicated naked thunk + adapter reusing
`HandleAotIndirectTransfer` from the saved guest `CONTEXT`, generalized fallback-cause
accounting shared with the RET path, and a synthetic probe. Also landed the Task 281
attempt-accounting fix (report `attempt = success + fallback`).

The VS2022/VS2026 Debug builds and every probe pass, including the new
`dbt_indirect_dispatch_all`. With indirect dispatch OFF (default), `aot-dbt` matches Task
281: graceful timeout, `entry=attempt=7141` with `940+6201`, VEH indirect boundary 8,891,
progress 29,782, zero fatal, unchanged EEPROM.

Enabling it deterministically crashes the Glide attract path (AV 0xC0000005 in the Glide
DLL, garbage `0xEF6ADDDD`). Controlled experiments ruled out the layout change
(force-fallback clean), inline-cache patching (no-patch still crashes), and FPU/SSE clobber
(FXSAVE unchanged). The first 30 dispatch `src/op/esp` sequences match ON vs OFF exactly and
the crash follows a CS-prefixed jump table right after, so the corruption is cumulative and
invisible in integer state. Because the success transfer's final register/stack state is
provably identical to the VEH `CONTINUE_EXECUTION` path, the remaining hypothesis is a
side effect the host-dispatch skips by bypassing the main VEH dispatch. Cross-run pointer
comparison is confounded by differing heap bases.

The feature is kept opt-in and disabled by default (`REPIU_AOT_DBT_INDIRECT=1`); the
accounting fix and fallback instrumentation stay. Next is a deterministic single-run
observation (fixed base or trap-backend single-step) to isolate the missing side effect.
