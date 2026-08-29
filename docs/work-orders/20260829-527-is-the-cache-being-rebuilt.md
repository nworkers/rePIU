# Task 527 작업 지시 — 캐시 재구축 여부 측정

설계: [20260829-527](../design/20260829-527-is-the-cache-being-rebuilt.md) ·
작업 로그: [20260829-527](../work-logs/20260829-527-is-the-cache-being-rebuilt.md)

## 1. 기존 카운터를 찾고 나서 만드십시오

`aot_retired_entry_trap_count`·`aot_quarantine_count`·`aot_dynamic_attempt_count`가 이미
있습니다. `grep -rn "fetch_add"`로 먼저 확인하십시오.

## 2. 값이 아니라 비율로 판정하십시오

`retired=133`은 그 자체로 크지도 작지도 않습니다. **`reentry=454450`과 나란히 놓아야**
재구축 가설이 서는지 무너지는지 보입니다.

## 3. 히스토그램은 가장 차가운 칸을 밀어내야 합니다

빈 칸만 채우고 얼면 기동 시점의 일회성 자리만 담깁니다. 526이 그렇게 만들어졌고 정상 상태를
전혀 보지 못했습니다.

**밀어낸 칸의 수는 1부터 다시 셉니다.** 따라서 이 sketch의 수는 하한이고, 정확한 순위표가
아닙니다. 결론을 그 이상으로 쓰지 마십시오.

## 4. 검증

양쪽 호스트 빌드. `[repiu-live-aot]`에 `retired`·`quarantine`·`translate`가,
`[repiu-live-site]`에 상위 자리가 나와야 합니다.
