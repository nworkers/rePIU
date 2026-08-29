# Task 525 작업 지시 — 핸들러를 체인 끝으로

설계: [20260829-525](../design/20260829-525-breakpoint-chain-order.md) ·
작업 로그: [20260829-525](../work-logs/20260829-525-breakpoint-chain-order.md)

## 1. 자리는 두 경계 사이입니다

`HandleSingleStepTrace` 뒤, `// Task 325: everything past this point ...` 앞.
**둘 중 하나라도 어기면 의미가 달라집니다** — 앞서면 센티널을 가로채고, 늦으면 디코드
체인이 `0xCC`를 지원하지 않는 명령으로 보고한 뒤입니다.

## 2. `fault`를 넘기십시오

AOT 블록 밖이므로 `aot_fault`는 범위 밖입니다. `aot_fault`는 `fault`의 복사본이니
(`const FaultEvent aot_fault = fault;`) 동작은 같습니다.

## 3. 센티널 검사를 지우지 마십시오

옮기면 `HandleSingleStepTrace`가 앞서므로 중복처럼 보입니다. **아닙니다.** 그 호출은
`kSingleStep`이거나 `aot_reentry_pending`인 `kBreakpoint`일 때만 일어나므로, 평범한
breakpoint로 발화한 센티널은 이 함수까지 옵니다. 주석에 그 이유를 적으십시오.

## 4. 주석을 사실로 만드십시오

옮기기 전 주석은 "after every handler that could own an INT3 has declined"라고 적어 놓고
다섯 핸들러 앞에 있었습니다. **이 작업의 절반은 그 문장을 참으로 만드는 것입니다.**

## 5. 검증

* 양쪽 호스트 빌드.
* WSLg 3회 측정. 이동은 성능 중립이어야 하므로 [Task 524](../work-logs/20260829-524-wslg-baseline-remeasure.md)의
  35.43 fps와 겹쳐야 합니다. 크게 벗어나면 무언가를 가로채고 있다는 뜻입니다.
