# 게스트 스택 워치포인트 시도 작업 로그 (negative result)
# Work Log: Guest Stack Watchpoint Attempt (Negative Result)

## 1. 범위 (Scope)

작업 지시서 `docs/work-orders/20260716-223-guest-stack-slot-corruption-watchpoint-
order.md`에 따라 Task 222가 확정한 게스트 스택 슬롯 `0x035D6B14`의 비동기 손상
writer를 워치포인트로 포착 시도했다. 저비용 사전 확인, 하드웨어 DR0/DR7, 소프트웨어
페이지 보호 순으로 시도했으며 상세 설계·실패 분석은
`docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md`에 있다.

## 2. 저비용 사전 확인 (완료, 유효한 결과)

로더 로그 실측(`Guest stack switch stack base/limit: 0x03110000/0x035D6E60`,
`Runtime memory arena end: 0x0B5D7000`대, 128MB slack 반영)으로 게스트 스택과
LINEXE 합성 영역이 정적으로 겹치지 않음을 확인했다 — **작업 지시서 후보 3(메모리
레이아웃 겹침) 배제**. `dynamic_allocator_base`(heap 시작)가 스택 상단에서 불과
`0x1A0`바이트 위라는 인접성은 관찰했으나, 손상 슬롯은 그 경계 아래(스택 쪽)라 직접
겹침은 아니다.

## 3. 하드웨어 DR0/DR7 시도 (실패)

설계·구현 후 실행 결과, `SetThreadContext`/`GetThreadContext` 설치·리드백은
완전히 성공했으나 실제 타겟 주소로 구동 시 게스트 dispatch가 시작되기 전
프로세스가 `STATUS_SINGLE_STEP`(0x80000004) 원시 종료코드로 즉사했다. VEH
무조건 진입 로그(파일 직접쓰기)가 한 줄도 기록되지 않아 VEH 자체가 호출되지
않음을 확인했다. 죽은 주소로는 60초+ 정상 구동 — 실주소가 원인임은 확실하나
VEH 미호출의 근본 기제는 미확정.

## 4. 소프트웨어 페이지 보호 시도 (근본 원인 규명, 그러나 여전히 실패)

이 저장소의 기존 AOT self-modifying-code 감지(`HandleAotGuestCodeWriteFault`/
`Completion`)와 동일한 패턴으로 재구현했다. 근본 원인을 다음과 같이 규명했다:
**타겟 주소(`0x035D6B14`)가 게스트 진입 ESP(`0x035D6E58`)에서 불과 836바이트
아래이며, Windows 예외 디스패치 자체(CONTEXT+EXCEPTION_RECORD, ~1KB)가 VEH를
호출하기 위해 현재 ESP 아래쪽에 그 부기 정보를 쓰는데, 이 범위가 보호된 페이지와
겹치면 VEH 호출 자체가 실패**한다 — DR 방식 실패와 동일한 근본 원인으로 재해석된다.

ESP가 타겟보다 안전하게 낮아진 뒤로 설치를 지연하자 최초 즉사는 해소되어 12.45초
정상 구동, **Task 222와 정확히 일치하는 종료 상태**(`EDI=0xDD1523B1`,
`ESI=0x032953AC`, `"01.tga"`)로 깨끗하게 재현했다 — 다만 설치 조건이 그 구동
내내 충족되지 않아 워치포인트 자체는 비활성으로 남았다(**버그 재현 확인, 포착은
실패**). margin을 줄여 실제 무장이 발생하게 하면 ESP가 단조 감소하지 않아(호출/
반환으로 재상승) 이후 다른 예외 디스패치 시점에 동일한 방식으로 재현 가능한
즉사가 발생했다. 예외마다 ESP를 재확인하는 동적 무장/해제로도 동일 지점에서
즉사가 재현됐고, 한 케이스는 무장/해제 로그가 전혀 없었는데도(보호가 걸리지
않았는데도) 동일 지점에서 즉사해 — 우리 코드의 미세한 타이밍 섭동이 AOT 백그라운드
워커와의 기존 경쟁 조건을 드러냈을 가능성도 배제할 수 없다.

## 5. 결론 (Conclusion)

이 타겟 주소는 실행 전반에 걸쳐 다수의 무관한 예외 디스패치 지점의 ESP와 반복적으로
근접한다 — 구조적 특성이며 단발성 위험 구간이 아니다. **스택 데이터 주소를 감시하는
접근(하드웨어·소프트웨어 모두)은 이 타겟에 근본적으로 안전하지 않다.** 두 구현 모두
되돌렸다(커밋하지 않음) — 브랜치 `feature/223-guest-stack-slot-corruption-
watchpoint`에 diff 형태로만 남아 있고 머지하지 않았다.

## 6. 다음 권고 (Recommendation)

Task 222가 corruption window를 두 게스트 **코드** 주소로 이미 확정했다(store
`0x03021F41` 직후, load `0x03021F71` 직전, 사이에 call/push/pop 없음). 스택
주소 대신 **코드 주소**를 감시하면 ESP 근접 위험이 원천적으로 없다. 기존
`REPIU_EXECUTION_PROBE_OFFSET`/`RecordExecutionProbe` 단발성 코드 주소 트리거
인프라를 두 지점 반복 발화로 확장하고 `[esp+0x154]`를 매 iteration 스냅샷하면,
값이 실제로 바뀌는 iteration을 특정한 뒤 근인을 좁힐 수 있다. 새 work-order로
분리 진행을 권고한다. 상세는
`docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md` §7 참고.

## 7. 후속: 코드 주소 이중 프로브로 부분 성공 (같은 세션)

§6 권고를 같은 세션에서 즉시 구현했다. `REPIU_EXECUTION_PROBE_OFFSET`/
`RecordExecutionProbe`의 단발 게이트를 제거하고 범위 링버퍼(`RecordExecutionTrace`,
`REPIU_EXECUTION_TRACE_START/END/ESP_OFFSET`/`REPIU_EXECUTION_TRACE_SENTINEL2` env)로
확장해 `0x03021F41`(store)과 `0x03021F71`(load)에 독립 int3 sentinel을 설치하고
aot-dynamic으로 구동했다. **결과: 종료 예외를 유발한 바로 그 호출에서 load 지점
도달 시점에 이미 `[esp+0x154]`가 `0xDD1523B1`이었다** — load는 이미 손상된 값을
읽었을 뿐이다. 다른 호출에서는 같은 지점에서 정상 값(`0x0325E1F8`)을 확인했다.
손상 구간이 "store 완료 이후 ~ 문제의 그 호출이 load에 도달하기 전"으로 좁혀졌다.
`0x03021F41` sentinel이 문제의 호출에서는 재발화하지 않아 그 호출의 "store 직후"
상태는 아직 미확보다. trap 백엔드(sentinel 불필요)로 재시도했으나 150초 예산 안에
종료 지점에 도달하지 못했다. 상세는
`docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md` §8~9 참고.
이 구현은 브랜치에 커밋하지 않고 diff로 남겨 다음 세션이 이어받을 수 있게 했다.

## 8. 후속: 재발화 비대칭은 구조적 한계로 결론 (다음 세션)

§7의 열린 질문(`0x03021F41` sentinel이 왜 최초 1회만 발화하는지)을 추적했다.
`aot_retired_entry_trap_count`가 0으로 유지됨을 확인해 캐시 세대 retire 가설을
배제했고, 매 hit마다 무조건 sentinel을 재설치하는 자기재무장을 추가해도
(재무장 자체는 3회 실행됨을 계측으로 확인) store는 여전히 재발화하지 않음을
확인했다. 함수 진입점(`0x03021DF8`)으로 sentinel 위치를 바꿔도 동일한 "최초
1회만" 패턴이 재현되어, store 명령 자체의 특수성이 아니라 **"boundary/reentry
이벤트로 도달한 주소는 이후 호출에서 그 캐시 사본을 다시 지나가지 않는다"**는
더 일반적인 구조적 제약임을 확인했다(가장 유력한 설명: 호출자 인라인 캐시가
최초 이벤트에서 직접 타겟을 학습해 이후 우리가 패치한 사본을 우회함). 반면
순수 fall-through로만 도달하는 load 지점은 안정적으로 매 호출 재발화한다.

**결론: 이 int3 sentinel 기법으로는 "문제의 그 호출"에서 store 직후 상태를
관측할 수 없다** — load 지점(이미 손상된 이후)만 반복 관측 가능하다. sentinel
기반 접근은 이 지점에서 소진됐다고 판단하고, 다음 두 방향 중 하나로 전환을
권고한다: (1) trap 백엔드를 더 긴 예산(≥180초)으로 재시도, (2) 비동기 writer
가설(다른 스레드/타이머/HLE trap 스택 오버랩)을 직접 조사. 자기재무장 코드는
원래 목적은 달성하지 못했지만 retire 경로 정확성을 개선하므로 그대로 유지했다.
상세: `docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md` §10.

빌드 환경 메모: `build/vs2022_debug`, `build/vs2022_win32_debug` 트리는 시스템
CMake 4.3(`C:/Program Files/CMake/bin/cmake.exe`)이 캐시에 고정되어 있어
재구성 시 VS 툴체인과 버전이 맞지 않아 실패한다(`vs2022_debug`는 추가로 x64로
잘못 구성되어 있기도 하다). 이번 검증은 `build/win32_x86_dpmi`(캐시가 VS 2026
Community 번들 cmake를 가리킴, Win32 플랫폼)에서 수행했다 — `cmake --build
build/win32_x86_dpmi --config Debug --target repiu_loader_win32`(및
`repiu_supervisor_win32`)로 정상 빌드됨을 확인했다. 이는 이 세션의 코드
변경과 무관한, 기존에 존재하던 빌드 환경 문제다.

---

**English summary.** Both hardware (DR0/DR7) and software (page-protection) guest-
stack watchpoints were implemented and empirically failed. The root cause for the
software approach was pinned down: the target address is repeatedly close to the ESP
of unrelated exception-dispatch points throughout execution (not a one-off risk
window), because Windows' own exception-dispatch frame construction writes downward
from the current ESP just to invoke the VEH — the same underlying reason the hardware
approach's VEH was never invoked either. One run with a safe install-timing margin did
cleanly reproduce Task 222's exact terminal fault (confirming the bug is real and
reachable) but never armed the watchpoint, so nothing was captured; smaller margins and
a dynamic per-exception re-arm/disarm both still hit a reproducible crash. Both
implementations were reverted (never committed; only exist as an uncommitted diff on
`feature/223-guest-stack-slot-corruption-watchpoint`). Recommended follow-up: watch the
two already-known *code* addresses (store/load bounding the corruption window) via this
codebase's existing one-shot execution-probe infrastructure extended to fire
repeatedly, avoiding the ESP-proximity risk entirely — proposed as a separate work
order. Details in `docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md`
§7.

**§8 follow-up (English).** Ruled out cache retirement (the retirement counter stayed 0)
and then ruled out simple byte loss too, via unconditional self-re-arm on every sentinel
hit (confirmed executing 3 times) — the store sentinel still never re-fired. Moving the
sentinel to the function entry (`0x03021DF8`) reproduced the identical one-shot pattern,
ruling out anything specific to the store instruction: only addresses reached by pure
fall-through (like the load) are repeatedly observable with this technique; addresses
treated as reentry/boundary targets are never revisited by later calls' execution paths
(most likely because the caller's inline cache learns a direct fast-path target after the
first such event). Conclusion: this technique cannot observe the post-store state for the
specific crashing call — sentinel-based narrowing is exhausted here. Next: retry the trap
backend with a longer budget, or pivot to directly investigating the async-writer
hypothesis. Build note: verification this round used `build/win32_x86_dpmi` (Win32
platform, VS 2026 Community's bundled cmake) since `build/vs2022_debug` and
`build/vs2022_win32_debug` have a stale/mismatched system-CMake-4.3 cache unrelated to
this session's code changes. Details in
`docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md` §10.
