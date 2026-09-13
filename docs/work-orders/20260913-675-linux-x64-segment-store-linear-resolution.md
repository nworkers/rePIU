# 작업 지시서: Linux x64 segment-store HLE linear resolution

## 한국어

설계: [20260913-675](../design/20260913-675-linux-x64-segment-store-linear-resolution.md)

### 작업 목표

planner-HLE의 `MOV r/m16, Sreg` memory form이 guest DS/ES selector base를
반영해 올바른 guest linear address에 쓰도록 수정합니다. Task 674의 공통
fail-closed gate와 결합해 resolver 실패 시 원본 long-mode 명령을 실행하지
않도록 합니다.

### 구현 순서

1. 일반 segment-store memory form을 guest DS selector로 resolve합니다.
2. 명시적인 ES override form을 guest ES selector로 resolve합니다.
3. 기존 write/protection/trace 경로를 유지합니다.
4. core probe와 Linux x64 runtime을 빌드합니다.
5. 초기 planner-HLE 경계를 bounded 실행으로 확인하고 다음 frontier를 기록합니다.
6. 분석 문서와 작업 로그를 갱신합니다.

### 완료 조건

* `MOV [disp32], ES`가 DS base를 누락한 raw offset으로 처리되지 않습니다.
* 특정 guest EIP 목록 없이 segment-store memory form 전체에 적용됩니다.
* resolver 실패 시 Task 674 gate가 원본 non-identical bytes 실행을 차단합니다.
* 기존 core probe가 통과하고 bounded runtime이 다음 실행 frontier까지 진행합니다.

## English

Design: [20260913-675](../design/20260913-675-linux-x64-segment-store-linear-resolution.md)

### Objective

Make planner-HLE's memory form of `MOV r/m16, Sreg` resolve the guest DS/ES
selector base before writing, while retaining Task 674's common fail-closed gate
when resolution cannot be proven.

### Implementation order

1. Resolve ordinary segment-store memory forms through guest DS.
2. Resolve the explicit ES override form through guest ES.
3. Preserve the existing write, protection, and trace path.
4. Build the core probe and Linux x64 runtime.
5. Run the bounded runtime through the initial planner-HLE frontier and record
   the next frontier.
6. Update the analysis and work log.

### Done when

* `MOV [disp32], ES` no longer treats its offset as an unbased linear address.
* The change applies to the segment-store memory family rather than one guest
  EIP.
* Resolver failure cannot fall through to non-identical original bytes on x64.
* Existing probes pass and the bounded runtime reaches the next frontier.
