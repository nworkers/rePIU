# AOT quarantine 원인 실시간 계측 작업 로그
# Work Log: Live Telemetry for the AOT Quarantine Cause

## 1. 개요 (Overview)

Task 217이 남긴 질문("`0x030EE1DA`가 속한 페이지의 quarantine이 오탐인지 DOS4GW 자체의 정상
self-modify인지")에 답하기 위해 `aot_last_retired_page`/`aot_last_code_write_source`/
`aot_last_code_write_destination`을 실시간 미러링했다(설계: `docs/design/20260716-218-aot-
quarantine-cause-live-telemetry.md`).

## 2. 구현 (Implementation)

1. `include/repiu/platform/win32/live_telemetry.h`: 세 필드 추가, 버전 13 → 14.
2. `src/platform/win32/execution_trampoline.cpp`: 기존 `store` 호출 두 지점(페이지 retire 성공
   시, 코드 write 관측 시)에 `InterlockedExchange` 미러링 추가.
3. `src/host/win32/supervisor_main.cpp`의 `PrintSnapshot`에
   `retire_page=0x.. write_src/dst=0x../0x..` 출력 추가.
4. Debug 재빌드 성공.

## 3. 검증 결과 (Verification Results)

`REPIU_EXECUTION_BACKEND=aot-dynamic`으로 `pumpit1`을 25초 재구동했다(`task218-verify-25s.log`).

| elapsed_ms | quarantine 누적 | retire_page | write_src -> write_dst |
|---|---|---|---|
| 10265 (첫 이벤트) | 0 | `0x030FE000` | `0x030F3432` -> `0x030FED0F` |
| 11297 | 0 | `0x030FE000` | `0x030F3432` -> `0x030FEDB4` |
| 13359 | 0 | `0x030FE000` | `0x030F3432` -> `0x030FED46` |
| 16422 (안정화) | 36 | `0x030FE000` | `0x030F3432` -> `0x030FECC4` |
| 20562 (동결 시작, bguest=`0x030EE1DA`) | 36 (불변) | `0x030FE000` (불변) | 불변 |

모든 quarantine 이벤트가 페이지 `0x030FE000`에서, 쓰기 소스 `0x030F3432`(목적지는 이벤트마다
다름)에서 발생했다. `0x030F3432`는 Task 209 분석(`docs/analysis/20260715-209-aot-dynamic-import-
stub-storm.md`)이 이미 역어셈블한 DOS4GW cross-segment thunk 패처의 `mov [edi+0x01], eax`
명령(aot_probe 주소 `0x010F3432`)과 정확히 일치한다.

## 4. 결론 (Conclusion)

**quarantine은 오탐이 아니라 DOS4GW 자신의 정상적인 cross-segment thunk 자기 패치다.** 여러
호출부의 thunk 스텁이 같은 4 KiB 페이지(`0x030FE000`)에 밀집해 있어, 새 호출부가 처음
해석될 때마다 그 페이지에 대한 quarantine 판정이 반복 트리거된 것이다.

그러나 **이 페이지는 `0x030EE1DA`가 속한 페이지(`0x030EE000`)와 다르다** — 64 KiB 차이가 나는
별개 페이지다. Task 217이 세운 "quarantine이 `0x030EE1DA`의 캐시 진입을 막고 있다"는 인과 설명은
이번 계측으로 **성립하지 않음이 확인됐다**. `docs/analysis/current-execution-frontier.md`에
Task 218 항목을 추가해 Task 217의 인과관계 주장을 정정했다.

## 5. 다음 단계 (Next Steps)

quarantine이 배제된 이상, `0x030EE1DA` 동결의 남은 유력 후보는 AOT 자체의 call/return 프레임
매칭 실패다(`docs/analysis/aot-return-stack-divergence.md`와 같은 계열). `aot_call_depth`/
`aot_call_frames`/`aot_last_return_matches_call`/`aot_last_call_source`/`aot_last_call_target`/
`aot_last_expected_call_source`/`aot_last_expected_call_target`(`execution_trampoline.cpp:195-
202`)를 같은 방식으로 라이브 미러링해, 이 RET로의 호출/반환 쌍이 매번 프레임 매칭에 실패하는지
직접 확인하는 것이 다음 계측이다.

## 6. 참고 (References)

* 로그: `task218-verify-25s.log`(세션 스크래치패드, UTF-16LE → `task218-verify-25s.utf8.txt`)
* 관련 문서: `docs/design/20260716-218-aot-quarantine-cause-live-telemetry.md`,
  `docs/analysis/aot-return-stack-divergence.md`,
  `docs/analysis/current-execution-frontier.md`(Task 217, Task 218 항목)
