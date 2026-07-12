# 단계적 장기 실행 관찰 작업 지시 / Work Order

## 한국어

1. 3분 실행으로 장기 진행 상태와 새 blocker를 확인한다.
2. 계속 진행하면 5분으로 늘려 반복 구간과 telemetry 추세를 비교한다.
3. 여전히 진행 중이면 10분 관찰로 main loop·정상 종료·새 frontier를 확인한다.
4. 확인된 HLE 누락은 설계 후 구현·빌드·재검증한다.
5. analysis와 작업 로그를 갱신한다.

## English

1. Run for 3 minutes to identify sustained progress or a new blocker.
2. If progress continues, extend to 5 minutes and compare recurring regions and telemetry trends.
3. If still active, observe for 10 minutes for a main loop, normal exit, or new frontier.
4. Design, implement, build, and reverify any confirmed HLE gap.
5. Update analysis and the work log.
