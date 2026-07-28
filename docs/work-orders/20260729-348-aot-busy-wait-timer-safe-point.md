# 20260729-348 AOT busy-wait 타이머 safe point 작업 지시 / Work order

## 한국어

### 목표

`0x0302FA08..0x0302FA10`에서 재현된 AOT 무경계 타이머 대기를 일반적인 back-edge
safe point로 해소합니다. 원본 코드와 ISR은 유지하고 guest thread VEH에서만 INT 8을
주입합니다.

### 작업

1. AOT code-cache image에 timer safe-point build option과 site metadata를 추가합니다.
2. direct/conditional/fallthrough back edge 앞에 flags 보존 request guard를 생성합니다.
3. 초기 placement와 dynamic append에서 request 주소와 trap index를 해결합니다.
4. poll thread가 pending tick과 함께 placement request를 게시하게 합니다.
5. timer boundary 모듈에 safe-point breakpoint handler를 추가하고 일반 AOT reentry보다
   먼저 호출합니다.
6. site/trap/injected/deferred 진단값을 실행 결과와 loader 로그에 추가합니다.
7. AOT probe에 생성 형식과 요청 on/off 실행 검증을 추가합니다.
8. `ARCHITECTURE.md`, `docs/analysis/interrupts-and-port-io.md`에 확인 결과를 반영합니다.
9. Win32 x86 Debug 빌드와 무입력/입력 재현 검증을 수행합니다.

### 완료 조건

- pending이 없을 때 safe point 전후 GPR/EFLAGS/ESP와 분기 결과가 동일합니다.
- pending이 있으면 guest thread VEH가 safe point를 식별합니다.
- IF=1이면 기존 공용 주입기로 원본 INT 8 ISR에 진입합니다.
- 재현 루프에서 tick이 1을 넘어 진행합니다.
- fatal, malformed exception dispatch, `ESP-12`, host `0x80000004`가 없습니다.
- 작업 로그와 관련 누적 문서가 갱신됩니다.

---

## English

### Goal

Resolve the reproduced boundary-free timer wait at
`0x0302FA08..0x0302FA10` with general AOT back-edge safe points. Preserve the
original code and ISR, and inject INT 8 only from guest-thread VEH.

### Work

Add code-cache options and metadata, emit flag-preserving guards at direct,
conditional, and fallthrough back edges, resolve request addresses for initial
and dynamic placement, publish requests from the poller, handle exact
safe-point breakpoints before generic AOT reentry, expose diagnostics, add a
synthetic AOT probe, update architecture/analysis, and perform Win32 x86 Debug
plus no-input and interactive reproduction verification.

### Completion

The inactive path preserves machine state and branch results. The active path
reaches guest-thread VEH and the existing injector, advances the reproduced
tick beyond 1, and introduces no fatal, malformed dispatch, `ESP-12`, or host
`0x80000004` failure. Documentation and the work log are complete.
