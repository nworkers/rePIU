# Zero return-slot 증거 덤프 설계 / Zero Return-Slot Evidence Dump Design

## 목적 / Purpose

Task 245가 특성화한 zero-EIP 종료(guest `0x0304ED35` RET이 `[ESP]=0`을 pop)는 약
75초에 결정적으로 재현된다. 근인(누가 호출자 반환 슬롯을 0으로 만드는가)을 확정하기
위해, 실패 시점에 자체 완결적인 증거 패킷을 stderr로 덤프한다.

The Task 245 zero-EIP termination (the RET at guest `0x0304ED35` popping `[ESP]=0`)
reproduces deterministically near 75 seconds. To pin what zeroes the caller-return
slot, dump a self-contained evidence packet to stderr at the moment of failure.

## 정책 / Policy

- 트리거: `HandleAotReturnTransfer`가 `[ESP]`에서 읽은 반환 target이 0일 때
  (정상 실행에서 반환 주소 0은 항상 병리적이다). 최초 4회만 덤프한다.
- 덤프 내용:
  1. 게스트 스택 `[ESP-0x20 .. ESP+0xDC]` 64 dwords (0 슬롯이 고립 스토어인지
     연속 클리어(memset류)인지 판별).
  2. RET 주변 라이브 코드 창 96바이트(`Eip-0x40 .. Eip+0x20`) — 이 영역은 런타임
     기록 코드라 정적 이미지와 다르므로 라이브 덤프가 필수.
  3. `aot_call_frames` 상위 8개(source/target/fallthrough)와 depth — dispatcher가
     추적한 호출 체인으로 이 프레임의 정당한 반환 주소 후보를 식별.
  4. 직전 return trace 8개 항목.
- 게스트 상태를 변경하지 않는 관측 전용 진단이며, 기존 fail-closed 경로(대상 해석
  실패 → false 반환)는 그대로 유지한다.

- Trigger: `HandleAotReturnTransfer` reading a zero return target from `[ESP]`
  (a zero return address is always pathological); dump at most four times.
- Contents: 64 guest-stack dwords around ESP (isolated store vs contiguous clear),
  a 96-byte live code window around the RET (runtime-written region, so live bytes
  are mandatory), the top eight tracked `aot_call_frames` with depth, and the last
  eight return-trace entries.
- Observation only: guest state is untouched and the existing fail-closed path is
  preserved.
