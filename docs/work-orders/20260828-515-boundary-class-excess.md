# Task 515 작업 지시 — 초과 경계의 종류

설계: [20260828-515](../design/20260828-515-boundary-class-excess.md) ·
작업 로그: [20260828-515](../work-logs/20260828-515-boundary-class-excess.md)

## 1. 새로 세지 마십시오

`veh_gap_counts`와 `veh_gap_cycles`는 이미 세 칸 다 채워집니다. 512가 single-step만 찍었을
뿐입니다.

## 2. `[repiu-live-veh]` 줄을 넓히십시오

`gap_bp_count`·`gap_bp_mean`·`gap_other_count`·`gap_other_mean`을 더합니다. 버퍼 크기를
확인하십시오 — 지금 320바이트입니다.

세 클래스의 합이 `veh_count`와 대략 맞아야 합니다. **맞지 않으면 그 자체가 소견이고**, 해석
전에 왜인지 보십시오.

## 3. 양쪽 호스트에서 3회씩

조건은 509 이후로 같습니다 — Release, vsync OFF, 감시견 OFF, `pumpit1`, 90초.

**Windows는 요약에도 같은 값이 있습니다**(`VEH gap counts single-step/breakpoint/other`).
새 줄이 그것과 어긋나면 새 줄이 틀린 것입니다.

## 4. 판정

클래스별 **프레임당 배달 수**로 비교하십시오. 비율만 보면 512가 경고한 함정에 다시 빠집니다 —
분모가 다르면 같은 절대량이 전혀 다르게 보입니다.

## 5. 하지 마십시오

* 고치지 마십시오. 차단된 세 native 경로를 켜지도 마십시오.
* 하위 버킷으로 내려가지 마십시오.
