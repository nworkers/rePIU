# 20260726-308 작업 지시: exception-free superblock 아키텍처 검증 / Work order: exception-free superblock architecture validation

설계: [20260726-308-exception-free-superblock-validation.md](../design/20260726-308-exception-free-superblock-validation.md)

## 한국어

### 작업

- [x] 최신 `aot-dynamic`/`aot-dbt` 30초 기준선과 예외 provenance를 고정합니다.
- [x] 기존 planner/emitter의 cache-local direct/conditional/fallthrough/backedge 연결을 확인합니다.
- [x] opt-in HLE host-dispatch site metadata와 fail-closed slot을 구현합니다.
- [x] host stack/TIB 전환, GPR/EFLAGS 및 x87/MMX/SSE 보존 thunk를 구현합니다.
- [x] 기존 공용 HLE handler chain을 재사용하고 segment-write와 `INT/IRET`를 제외합니다.
- [x] entry/success/fallback 및 원인별 계측을 실행 결과와 로그에 추가합니다.
- [x] emitter/coverage/thunk 합성 probe와 기존 전체 probe를 통과시킵니다.
- [x] Win32 x86 Debug 전체 빌드 후 실제 `pumpit1` OFF/ON A/B를 수행합니다.
- [x] 결과를 architecture, analysis, frontier와 작업 로그에 반영하고 커밋합니다.

### 완료 조건

옵트인 OFF는 기존 image byte와 실행 정책을 유지해야 합니다. ON의 모든 dispatch attempt는
success 또는 fallback으로 귀결되어야 합니다. 사전 fallback은 guest 상태를 변경하지
않고 기존 planner-HLE `INT3`로 들어가며, 처리 후 target miss는 next EIP의 TF bridge로
이어져야 합니다. 실제 실행은 exception/fatal/AOT legacy fallback 증가 없이 요청
시간을 완료하고 EEPROM hash가 일치해야 합니다. 측정 결과로
5배 whole-run go/no-go 충족 여부와 60배 목표에 남은 차이를 명시합니다.

## English

Implement an opt-in HLE host-dispatch slot and Win32 x86 thunk that preserve guest
integer, flags, and extended floating-point state, switch safely to the host stack,
reuse the established HLE handler chain, and return directly to an active cache target.
Exclude segment-register writes and `INT/IRET` in the first slice and preserve planner-HLE
`INT3` fallback. Add accounting probes, pass all existing probes and the Win32 x86
Debug build, then run a bounded real-game OFF/ON comparison. Record whether the result
meets the 5x whole-run architecture threshold and how far it remains from the 60x goal.
