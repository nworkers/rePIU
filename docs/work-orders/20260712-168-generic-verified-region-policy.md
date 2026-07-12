# 공용 verified-region 정책 작업 지시 / Work Order

## 한국어

1. direct CALL transition 관찰과 candidate cache를 추가한다.
2. 제한형 32-bit x86 instruction length/control-flow decoder를 구현한다.
3. reachable direct callees를 재귀 검증하고 민감·미확정 명령은 거부한다.
4. 기존 고정 offset fast path를 공용 candidate 정책으로 교체한다.
5. 단위 수준 synthetic byte 검증과 PIU 처리량을 확인한다.
6. architecture, analysis, 작업 로그를 갱신한다.

## English

1. Add direct-call transition observation and candidate caching.
2. Implement a bounded 32-bit x86 instruction-length/control-flow decoder.
3. Recursively verify reachable direct callees and reject sensitive or unknown instructions.
4. Replace the fixed-offset fast path with the generic candidate policy.
5. Verify synthetic byte cases and PIU throughput.
6. Update architecture, analysis, and the work log.
