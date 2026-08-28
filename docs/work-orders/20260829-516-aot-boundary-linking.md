# Task 516 작업 지시 — AOT 경계 연결

설계: [20260829-516](../design/20260829-516-aot-boundary-linking.md) ·
작업 로그: [20260829-516](../work-logs/20260829-516-aot-boundary-linking.md)

## 1. 새로 세지 마십시오

세 번째로 같은 말입니다. 사유별 다섯과 residency는 이미 채워집니다.

## 2. `[repiu-live-aot]` 줄을 더하십시오

`live_execution_profile_report.cpp`. `ThreadContext`의 atomic들을 읽어야 하므로 보고기
시그니처에 `const ThreadContext*`를 넘기는 대신 **필요한 값만 담은 작은 구조체**를 받으십시오 —
보고기가 실행 엔진 헤더에 의존하기 시작하면 계층이 역전됩니다.

## 3. 합이 맞는지 먼저 보십시오

다섯 사유의 합 == `boundary`. 어긋나면 **해석하지 말고** 왜인지부터 보십시오.

## 4. 양쪽 호스트 3회씩, 프레임당으로 비교

조건은 509 이후 그대로입니다. Windows 요약에도 같은 값이 있으므로(`Win32 AOT
entry/boundary/reentry/fallback`) 새 줄과 어긋나면 새 줄이 틀린 것입니다.

## 5. 하지 마십시오

* 고치지 마십시오.
* 사유가 가리키는 곳으로 바로 뛰어들지 마십시오 — 516은 이름을 붙이는 데까지입니다.
