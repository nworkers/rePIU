# AOT 반환 전이(return transfer) 실시간 계측 설계
# Design: Live Telemetry for AOT Return Transfers

## 1. 배경 (Background)

Task 218은 quarantine(페이지 `0x030FE000`, DOS4GW thunk 스텁 테이블)이 정상 동작임을 확인했지만,
동결 지점 `0x030EE1DA`(RET)와는 별개 페이지임을 밝혀 Task 217의 인과 설명을 기각했다.

`HandleAotReturnTransfer`(`execution_trampoline.cpp:9946`)를 정독하면 새 가설이 성립한다:

1. 이 핸들러는 RET의 반환 대상을 게스트 스택에서 읽고 `ResolveAotTransferTarget(target)`을
   호출하는데, **대상이 quarantine된 페이지 위에 있으면 Resolve가 캐시 조회 전에 즉시 실패**
   한다(`:9602-9606`). 이때 핸들러 전체가 `false`를 반환하고(`:10010-10013`) RET는 TF 단일
   스텝으로 네이티브 실행된다.
2. 단일 스텝 후 EIP가 quarantine 페이지 위에 있으면 `HandleAotReentry`의 SINGLE_STEP 분기가
   TF를 유지한 채 네이티브 단일 스텝을 계속한다(`:10111-10118`) — quarantine 페이지의 설계된
   실행 방식.
3. 실행이 quarantine 페이지를 벗어나 번역된 주소에 도달하면 Resolve가 성공해 캐시로 복귀한다
   (reentry 1회).

즉 **"번역된 함수 → RET → quarantine된 thunk 페이지로 반환 → 단일 스텝 통과 → 번역 코드
재진입"** 왕복이라면, (a) `aot_boundary_guest_eip`가 RET 주소 `0x030EE1DA`에 고정되고,
(b) boundary/reentry가 1:1로 증가하며, (c) `aot_return_dispatch_count`는 동결되고,
(d) quarantine 카운터는 변하지 않는 — 지금까지의 모든 관측이 한 번에 설명된다. 남은 확인은
RET의 실제 반환 대상이 정말 `0x030FE000` 페이지인지다. 이 값은
`aot_last_return_target`(`:9973`)에 매 사이클 기록되지만 라이브 미러링되지 않는다.

## 2. 목적 (Objective)

`aot_last_return_source`/`aot_last_return_target`/`aot_last_expected_return`/
`aot_last_return_matches_call`/`aot_return_dispatch_count`를 실시간 미러링해, 동결 구간에서
RET `0x030EE1DA`의 반환 대상 주소를 직접 관측하고 위 가설을 판정한다.

## 3. 설계 (Design)

1. `Win32SharedLiveTelemetry`에 다섯 필드를 추가하고 버전을 14 → 15로 올린다.
2. `HandleAotReturnTransfer`의 기존 기록 지점(`:9973-9977`, 프레임 매칭 판정 후, dispatch
   카운트 증가 지점)에 `InterlockedExchange`/`InterlockedIncrement` 미러링을 추가한다.
3. `PrintSnapshot`에 `ret_src/tgt/expected=0x../0x../0x.. ret_match=.. ret_dispatch=..`를
   추가한다.
4. 검증: 25초 재구동에서 동결 구간의 `ret_tgt`가 `0x030FE000~0x030FEFFF` 범위면 가설 확정.

## 4. 판정 후 방향 (Direction After the Verdict)

가설이 확정되면 이 churn 자체는 "멈춤"이 아니라 quarantine 페이지 경유로 인한 **속도 저하**다.
그 경우 progress의 실제 blocker는 이 루프가 무엇을 기다리는지(2026-07-14의 게임 내부 tick 폴링
가설)로 옮겨가며, 반환 대상 주변과 상시 관측되는 간접 전이(`0x030DAEC3 → 0x03085E9C`)를
`repiu_aot_probe`로 역어셈블해 루프 본문이 폴링하는 조건을 확인하는 것이 다음 단계다.
