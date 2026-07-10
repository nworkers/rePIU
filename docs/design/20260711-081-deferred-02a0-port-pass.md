# 0x02A0 Port I/O 보류 통과 설계

## 배경

`0x02A0` 계열 Port I/O trace에서 규칙적인 초기화 패턴이 관측되었지만, 실제 장치 의미는 아직 확정하지 않았다. 따라서 해당 패턴은 TODO로 남겼다.

현재 중단점은 실제 guest blocker가 아니라, 진단을 위해 둔 trace buffer 용량 제한이다. 다음 실제 요구사항을 확인하려면 `0x02A0..0x02AF` 범위의 4-byte `OUT DX,EAX`를 더 진행시켜야 한다.

## 설계

`0x02A0` 계열 의미 분석은 보류하되, 다음 blocker 관측을 위해 제한된 no-op 통과를 추가한다.

* `0x02A0..0x02AF` 범위의 4-byte `OUT DX,EAX`는 `deferred-ignored`로 처리한다.
* 전체 Port I/O 관측 수가 큰 안전 상한을 넘으면 `deferred-limit`으로 중단한다.
* trace buffer는 앞쪽 일부 시퀀스만 보존하며, trace buffer가 가득 차도 통과 자체는 멈추지 않는다.
* `IN` 계열 Port I/O는 응답 모델이 없으므로 기록 후 중단한다.
* 범위 밖 Port I/O도 기존처럼 unsupported로 중단한다.

## 기대 결과

`piu_1st`가 인위적인 trace-limit을 넘어 실제 다음 blocker에 도달해야 한다. 다음 blocker가 `IN`이면 그 port와 caller 위치를 기준으로 장치 응답 모델 필요 여부를 판단한다.

## 범위 밖

* `0x02A0` 계열 장치 의미 확정
* `IN` 응답 모델 구현
* 모든 Port I/O의 무조건 no-op 처리

# Deferred 0x02A0 Port I/O Pass Design

## Background

The `0x02A0` Port I/O trace showed a regular initialization pattern, but the actual device meaning is still not confirmed. That analysis has been deferred to TODO.

The current stop is not a real guest blocker; it is the trace buffer capacity limit introduced for diagnostics. To observe the next real requirement, the loader needs to continue through more 4-byte `OUT DX,EAX` operations in the `0x02A0..0x02AF` range.

## Design

Defer the `0x02A0` meaning analysis while adding a bounded no-op pass-through to reach the next blocker.

* Treat 4-byte `OUT DX,EAX` in the `0x02A0..0x02AF` range as `deferred-ignored`.
* Stop with `deferred-limit` if total Port I/O observations exceed a large safety cap.
* Keep the trace buffer as an early-sequence sample only; filling the trace buffer no longer stops execution.
* Record and stop on `IN` Port I/O because no response model exists yet.
* Keep out-of-range Port I/O as unsupported.

## Expected Result

`piu_1st` should move past the artificial trace-limit and reach the next real blocker. If the next blocker is `IN`, its port and caller location will guide whether a device response model is needed.

## Out Of Scope

* Determine the real `0x02A0` device meaning.
* Implement an `IN` response model.
* Treat every Port I/O as unconditional no-op.
