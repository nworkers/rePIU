# Task 704 작업 지시 — Linux x64 IRETD HLE

## 목표

Linux x64에서 INT 8 주입 frame에 logical guest CS를 기록하고, 원본 ISR 끝의 32-bit
`IRETD` 효과를 HLE하여 정상 guest continuation으로 복귀시킵니다.

## 작업 범위

1. Linux x64 INT 8 주입 전에 interrupted EIP의 executable guest selector를 해석합니다.
2. 검증된 12-byte interrupt-return frame만 소비하는 전용 IRETD handler를 추가합니다.
3. 공용 guest HLE dispatcher와 fault-level chain에 handler를 연결합니다.
4. 성공한 fault-level IRETD 뒤 공용 handled-boundary AOT 재진입을 수행합니다.
5. synthetic probe, Linux x64 빌드/core probe, 실제 `pumpit2a`로 검증합니다.
6. 아키텍처, 누적 분석, 작업 로그를 갱신합니다.

## 제외 범위

* 원본 INT 8 ISR body 재구현
* 16-bit IRET, privilege-level 전환, VM86 return
* timer cadence, pending/backlog 정책 변경
* i386 native IRETD 경로 변경

---

## English

### Objective

Write logical guest CS into Linux x64 INT 8 frames and HLE the 32-bit `IRETD`
effect at the end of the original ISR so execution returns to the guest
continuation.

### Scope

Resolve the executable guest selector before injection; add a fail-closed
12-byte IRETD-frame handler; connect it to shared and fault-level HLE; resume a
successful fault-level return through handled-boundary AOT reentry; add
synthetic and real-runtime verification; and update architecture, analysis, and
work-log documentation.

### Out of scope

Reimplementing the ISR body, 16-bit or privilege-changing IRET variants, timer
delivery policy changes, and changes to the i386 native IRETD path.
