# Task 495 작업 로그: JAMMA history 안전 정리

## 결과

Task 494 사용자 로그에서 2P 다섯 위치의 keycode 별칭 수정은 확인됐지만, 968개 입력 edge
중 712개가 `history-overflow`로 기록됐습니다. 원인은 이미 소비 완료된 edge도 고정
256-entry history에 계속 남겨 둔 것이었습니다.

pending timer due와 active IRQ0 replay frame이 요구하는 가장 오래된 timestamp를 기준으로
완료 history를 floor state에 compact하도록 수정했습니다. 두 기준이 모두 없을 때는 단조
증가하는 마지막 PIT due timestamp까지 정리합니다. 안전 정리량과 최대 보존 크기를 각각
`history-pruned`, `history-peak`로 노출하고, 실제 용량 초과와 과거 범위 조회를
`history-overflow`, `history-coverage-miss`로 분리했습니다.

## 검증

- Debug `repiu`, `repiu_aot_probe` 빌드 성공. 첫 전체 재컴파일은 120초 도구 제한에
  도달했으며 이어진 증분 빌드는 성공했습니다.
- 전용 timeline probe: `pruned=300`, `history-peak=1`, `history-overflow=0`,
  `coverage-miss=0`.
- `pumpit1/PIU.EXE` 전체 probe: exit code 0, 모든 probe 통과.
- 10초 pumpito smoke: 실행 제한 종료, exception false, stall timeout false,
  `history-overflow=0`, `history-coverage-miss=0`, `due-overflow=0`, `missing-due=0`,
  `frame-overflow=0`, `replay-reads=6896`.

## 최종 사용자 검증

약 120초 실제 실행은 SDL exit request로 종료됐고 execution exception, wall timeout,
stall timeout은 모두 false였습니다. 다섯 2P 위치의 press/release는 UpLeft `77/77`,
UpRight `97/97`, Center `124/124`, DownLeft `107/107`, DownRight `91/91`로 균형을 이뤘습니다.

최종 timeline은 `edges/history-pruned/history-peak=1006/1006/4`,
`replays/replay-reads/frames-retired=27814/275350/27812`였습니다. `history-overflow`,
`history-coverage-miss`, `due-overflow`, `missing-due`, `frame-overflow`는 모두 0입니다. 종료
순간의 active depth 2는 주입과 retire 차이와 일치합니다. Task 495와 선행 Task 492~494의
사용자 입력 검증을 완료했습니다.

---

# Task 495 Work Log: Safe JAMMA History Pruning

## Result

The Task 494 user log confirmed the keycode-alias correction for all five 2P positions, but 712 of
968 input edges were reported as `history-overflow`. Completed edges remained indefinitely in the
fixed 256-entry history.

Completed history now compacts into a floor state using the oldest timestamp required by pending
timer due entries or active IRQ0 replay frames. If neither exists, pruning advances through the
monotonically increasing last PIT due timestamp. `history-pruned` and `history-peak` report normal
retirement and retained size, while `history-overflow` and `history-coverage-miss` distinguish
capacity exhaustion from an older-than-floor query.

## Verification

- Debug `repiu` and `repiu_aot_probe` builds succeeded. The initial broad rebuild reached the
  tool's 120-second limit; the following incremental build completed successfully.
- Dedicated timeline probe: `pruned=300`, `history-peak=1`, `history-overflow=0`,
  `coverage-miss=0`.
- Complete `pumpit1/PIU.EXE` probe: exit code 0, all probes passed.
- Ten-second pumpito smoke: execution-budget exit, exception false, stall timeout false,
  `history-overflow=0`, `history-coverage-miss=0`, `due-overflow=0`, `missing-due=0`,
  `frame-overflow=0`, and `replay-reads=6896`.

## Final User Validation

An approximately 120-second live run ended by SDL exit request; execution exception, wall timeout,
and stall timeout were all false. Press/release counts were balanced for every 2P position:
UpLeft `77/77`, UpRight `97/97`, Center `124/124`, DownLeft `107/107`, and DownRight `91/91`.

The final timeline reported `edges/history-pruned/history-peak=1006/1006/4` and
`replays/replay-reads/frames-retired=27814/275350/27812`. `history-overflow`,
`history-coverage-miss`, `due-overflow`, `missing-due`, and `frame-overflow` were all zero. Active
depth 2 at shutdown matches the injection/retirement difference. This completes user-input
validation for Task 495 and its prerequisite Tasks 492 through 494.
