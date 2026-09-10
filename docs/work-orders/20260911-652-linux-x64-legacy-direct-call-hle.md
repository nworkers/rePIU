# 20260911-652 작업 지시: Linux x64 legacy fallback direct CALL HLE

설계: [20260911-652 설계](../design/20260911-652-linux-x64-legacy-direct-call-hle.md)

## 한국어

1. x64 legacy fallback의 `E8 rel32` direct CALL을 shared HLE에서 처리합니다.
2. guest 반환 주소, ESP, EIP와 기존 AOT call-frame bookkeeping을 보존합니다.
3. 합성 probe로 direct CALL의 guest-stack 의미와 범위 거부를 검증합니다.
4. Linux x64 Debug 빌드, 전체 core probe, 실제 `pumpit2a`를 실행합니다.
5. 아키텍처·분석·작업 로그를 갱신하고 커밋합니다.

## English

1. Handle `E8 rel32` direct CALL through shared HLE during x64 legacy fallback.
2. Preserve its guest return address, ESP, EIP, and existing AOT call-frame bookkeeping.
3. Probe guest-stack semantics and range rejection for the direct CALL.
4. Build Linux x64 Debug and run the complete core probe and real `pumpit2a`.
5. Update architecture, analysis, and the work log, then commit.
