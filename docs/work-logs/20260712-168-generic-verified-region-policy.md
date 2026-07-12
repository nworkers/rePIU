# 공용 verified-region 정책 작업 로그 / Work Log

## 한국어

실제 direct `CALL` transition을 candidate로 수집하고 reachable CFG와 direct callee를 재귀 검증하는 제한형 decoder 프로토타입을 작성했습니다. 실행 telemetry에 candidate, 거부 instruction, opcode 및 byte window를 추가했습니다.

PIU 실행에서 207개 fast-path entry/return과 0개 취소를 관찰했지만, 핵심 unpack graph에서 `29 CF`의 operand `CF`를 독립 opcode로 오인하는 instruction-length 오류를 확인했습니다. 안전하지 않은 승인을 방지하기 위해 프로토타입을 compile-time fail-closed로 비활성화했습니다. 다음 작업은 pinned Zydis를 도입해 자체 길이 decoder를 교체하는 것입니다.

## English

Implemented a bounded prototype that collects observed direct-call transitions and recursively verifies reachable CFG nodes and direct callees. Added candidate, rejected instruction, opcode, and byte-window telemetry.

PIU execution observed 207 fast-path entries/returns and zero cancellations, but the critical unpack graph exposed an instruction-length defect: operand byte `CF` in `29 CF` was treated as a separate opcode. The prototype is compile-time fail-closed to prevent unsafe approval. The next task is to replace the in-house length decoder with pinned Zydis.
