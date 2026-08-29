# Task 526 작업 지시 — 재진입 뒤 폴트 분류

설계: [20260829-526](../design/20260829-526-what-follows-a-trap-free-reentry.md) ·
작업 로그: [20260829-526](../work-logs/20260829-526-what-follows-a-trap-free-reentry.md)

## 1. 분류는 디스패처 **맨 앞**에서

`UnhandledBreakpointEvidence` 선언 직전. 핸들러가 `Eip`를 고쳐 쓰기 전이어야 도착 주소가
의미를 갖습니다.

## 2. 종류를 가르십시오

`kSingleStep`만 제외하고 나머지를 한 통에 담으면 **접근 위반이 breakpoint로 보고됩니다.**
이 구현에서 실제로 그렇게 만들었다가 `other=915`가 나와서야 갈랐습니다.
**"폴트가 왔다"로 "INT3가 남아 있다"를 주장할 수 없습니다.**

## 3. 무장은 한 번, 소비도 한 번

`direct_dispatch_trap_pending`을 분류 시 즉시 내리십시오. 안 그러면 한 디스패치가 여러 폴트에
중복 계상됩니다. 이미 무장돼 있는데 다시 무장하면 그 사이에 폴트가 없었다는 뜻이므로
`clean`입니다.

## 4. 폴트 핸들러 안에서 할당하지 마십시오

히스토그램은 고정 배열입니다.

## 5. `snprintf` 형식과 인자를 함께 고치십시오

인자만 늘리고 형식 문자열을 그대로 두면 **형식/인자 불일치**가 됩니다. 이 작업에서 실제로 한 번
그렇게 만들었고 컴파일러가 잡아 주었습니다. 순서도 형식과 같아야 합니다.

## 6. Python heredoc에서 `\n`을 쓰지 마십시오

C++ 문자열 리터럴에 넣으려던 `\n`이 **실제 줄바꿈**이 되어 리터럴이 두 줄로 갈라집니다.
이 세션에서 네 번 겪었습니다. `chr(92) + 'n'`으로 만들거나 편집 도구를 쓰십시오.

## 7. 검증

양쪽 호스트 빌드. Linux에서 `REPIU_EXECUTION_TIME_PROFILE=1`과
`REPIU_LIVE_PROFILE_INTERVAL_MS`로 `[repiu-live-gdd]`·`[repiu-live-site]`를 받으십시오.
