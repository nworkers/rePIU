# AOT 반환 전이 실시간 계측 작업 로그
# Work Log: Live Telemetry for AOT Return Transfers

## 1. 개요 (Overview)

Task 218 이후 남은 가설("RET `0x030EE1DA`의 반환 대상이 quarantine된 thunk 페이지라서
`ResolveAotTransferTarget`이 매번 실패한다")을 검증하기 위해 반환 전이 진단 5필드를 실시간
미러링했다(설계: `docs/design/20260716-219-aot-return-transfer-live-telemetry.md`).

## 2. 구현 (Implementation)

1. `include/repiu/platform/win32/live_telemetry.h`: `aot_last_return_source`,
   `aot_last_return_target`, `aot_last_expected_return`, `aot_last_return_matches_call`,
   `aot_return_dispatch_count` 추가, 버전 14 → 15.
2. `src/platform/win32/execution_trampoline.cpp`의 `HandleAotReturnTransfer`: 반환 trace 기록
   지점과 dispatch 카운트 증가 지점에 `InterlockedExchange`/`InterlockedIncrement` 미러링 추가.
3. `src/host/win32/supervisor_main.cpp`의 `PrintSnapshot`에
   `ret_src/tgt/expected=.. ret_match=.. ret_dispatch=..` 출력 추가.
4. Debug 재빌드 성공.

## 3. 검증 결과 (Verification Results)

`REPIU_EXECUTION_BACKEND=aot-dynamic pumpit1` 25초 재구동(`task219-verify-25s.log`):

| elapsed_ms | dispatch | ret_src → ret_tgt | ret_dispatch |
|---|---|---|---|
| 22719 (동결 구간) | 56859 | `0x030EE1DA` → `0x030EE300` | 2584 |
| 23734 | 56859 | `0x030EE1DA` → `0x030EE300` | 3410 |
| 24766 | 56859 | `0x030EE1DA` → `0x030EE300` | 4251 |
| 25078 | 56859 | `0x030EE1DA` → `0x030EE292` | 4460 |

1. **설계 가설 기각:** 반환 대상은 quarantine 페이지(`0x030FE000`)가 아니라 같은 페이지의
   `0x030EE292`/`0x030EE300` 두 주소를 교대한다.
2. **"동결"은 정지가 아니다:** `ret_dispatch`가 동결 구간에서 초당 약 820씩 증가 — 반환
   디스패치가 매 사이클 성공하며 게스트는 루프를 실제로 돌고 있다.

`repiu_aot_probe`로 `0x010EE270`/`0x010EE292`/`0x010EE300`(런타임 `+0x02000000`)을 역어셈블한
결과:

* `0x030EE170`~`0x030EE1DA`(RET)는 비트스트림 심볼 추출 헬퍼 — `[0x03141064]` 테이블 선택자,
  `0x033A516C`/`0x033A522C` 테이블, 256엔트리 탐색 루프, 16비트 창 비트 추출(`sar ebp, cl`).
* 호출부 2곳: `0x030EE28D`(`call 0x030EE170`, 반환 `0x030EE292`)과 `0x030EE2FB`(동일 대상,
  반환 `0x030EE300`). `0x030EE300` 이후는 비트 저장소 `[edi]`에서 소비 비트를 차감하는 에필로그.

## 4. 결론 (Conclusion)

동결의 실체는 **단일 엔트리 반환 인라인 캐시 스래싱으로 약 1000배 감속된 비트스트림(Huffman류)
디코드 루프**다. AOT 반환 thunk의 인라인 캐시는 예측 반환 주소를 1개만 저장하는데 실제 반환
대상이 2개를 교대하므로, 매 반환이 miss → `int3` → VEH 왕복 → 재패치 → 다시 miss를 반복하며
사이클당 VEH 왕복 1회의 비용으로 처리량이 초당 약 820회에 묶인다. Task 204가 처리량 후보로
기록한 "indirect inline-cache 다중화"의 정확한 실증 사례다. `docs/analysis/current-execution-
frontier.md`에 Task 219 항목을 추가했다.

## 5. 다음 단계 (Next Steps)

반환 thunk 인라인 캐시의 2~4엔트리 다중화 설계(AOT 캐시 emitter의 반환 thunk 바이트 시퀀스 +
`RequestAotInlineCachePatch` 프로토콜 확장). 처리량이 회복되면 이 디코드가 유한 자산 디코드인지
프레임 반복 오디오 디코드인지, 그리고 그 다음 frontier가 무엇인지 관측한다.

## 6. 참고 (References)

* 로그: `task219-verify-25s.log`, 역어셈블: `probe_10ee270.txt`/`probe_10ee292.txt`/
  `probe_10ee300.txt`(세션 스크래치패드)
* 관련 문서: `docs/design/20260716-219-aot-return-transfer-live-telemetry.md`,
  `docs/analysis/current-execution-frontier.md`(Task 216~219 항목),
  `docs/analysis/aot-indirect-transfer-dispatch.md`
