# 20260726-309 작업 지시: single-step hotspot cycle 귀속 / Work order: single-step hotspot cycle attribution

설계: [20260726-309-single-step-hotspot-cycle-attribution.md](../design/20260726-309-single-step-hotspot-cycle-attribution.md)

## 한국어

### 작업

- [x] opt-in 설정 parser와 기본 OFF 정책을 추가합니다.
- [x] 고정 용량 guest-EIP histogram과 RAII TSC scope를 구현합니다.
- [x] HLE/timer/native/TF outcome별 count와 cycle을 기록합니다.
- [x] count/cycle 상위 32와 coverage를 종료 snapshot에 추가합니다.
- [x] final 로그와 합성 probe를 추가합니다.
- [x] Win32 x86 Debug 빌드와 전체 AOT probe를 통과시킵니다.
- [x] 실제 `aot-dbt` 60초 profile에서 상위 주소와 cycle 비율을 확보합니다.
- [x] architecture, analysis, frontier와 작업 로그를 갱신하고 커밋합니다.

### 완료 조건

기능 OFF는 기존 실행 동작을 유지하고 histogram 할당·집계 비용을 유입하지 않아야 합니다. ON은
single-step 실행 결정을 변경하지 않고 `total = outcome count 합`, 주소별 count 합과
total의 일치, 상위 coverage 100% 이하, overflow 명시를 만족해야 합니다. 실게임은
exception/AOT legacy fallback 증가 없이 timeout하고 EEPROM hash가 일치해야 합니다.

## English

Implement an opt-in, allocation-free single-step guest-EIP profile with RAII TSC timing,
outcome attribution, independent top-32 count/cycle snapshots, final logging, and synthetic
invariant probes. Pass the Win32 x86 Debug build and full AOT probe, then run a controlled
60-second `aot-dbt` profile and document whether a loop owns enough measured cycles to justify
the next exception-free generation task.
