# 작업 지시: 호출 래퍼형 PIU10 MP3 feeder 일괄 전송

## 목표

직접 `out dx, al` 형태뿐 아니라 의미가 검증된 호출 래퍼형 PIU10 MP3 feeder에도 주소·프로파일 독립적인 frame-tail batching을 적용합니다.

## 작업 범위

1. frame batch API에 현재 guest stack pointer를 전달합니다.
2. 기존 직접 출력 matcher 뒤에 제한적인 호출 래퍼 matcher를 추가합니다.
3. arena와 AOT byte fast path에서 정확한 guest ESP를 전달합니다.
4. synthetic probe에 relocated wrapper 성공 및 손상된 call/stack 거부 사례를 추가합니다.
5. 관련 분석 문서와 작업 로그를 갱신합니다.
6. probe, 단위 테스트, Win32 Release 빌드와 실제 로그로 검증합니다.

## 완료 조건

- 기존 직접 출력 feeder probe가 그대로 통과합니다.
- 호출 래퍼형 feeder가 고정 주소나 target profile 조건 없이 인식됩니다.
- 구조 검증 실패 시 scalar 경로가 유지됩니다.
- 실제 실행에서 `batched`가 증가하고 MP3와 게임 진행이 동시에 유지됩니다.

---

# Work order: wrapped-call PIU10 MP3 feeder batching

## Objective

Apply address- and profile-independent frame-tail batching to semantically validated wrapped-call PIU10 MP3 feeders as well as direct `out dx, al` feeders.

## Scope

1. Pass the current guest stack pointer through the frame-batch API.
2. Add a restricted wrapped-call matcher after the existing direct-output matcher.
3. Supply the exact guest ESP from arena and AOT byte fast paths.
4. Add relocated-wrapper success and corrupted-call/stack rejection cases to the synthetic probe.
5. Update the related analysis document and work log.
6. Verify with the probe, unit tests, Win32 Release build, and a live log.

## Completion criteria

- Existing direct-output feeder probes continue to pass.
- A wrapped-call feeder is recognized without a fixed address or target-profile condition.
- Failed structural validation preserves the scalar path.
- A live run reports increasing `batched` bytes while MP3 and game execution progress concurrently.
