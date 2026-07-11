# DOS/16M loader symbolic replay 작업 지시

1. 기존 parser 결과를 내부 정수 구조로 전달해 MZ/BW symbolic state를 만든다.
2. MZ image를 복사하고 78개 relocation을 `original + L` 식과 provenance로 적용한다.
3. 16개 BW copy/BSS image를 구성하고 module selector mapping을 결정한다.
4. RSI-2 relocation 1,110개를 적용하고 before/after/provenance를 기록한다.
5. 각 module entry point와 executable image mapping을 검증한다.
6. resident router source와 `CS:[0x066A]` constraint를 계산해 유일한 provider mapping 여부를 판정한다.
7. deterministic replay report, 분석 문서, 검증 및 작업 로그를 남긴다.

# DOS/16M Loader Symbolic Replay Work Order

Build symbolic MZ/BW states, apply all MZ and RSI-2 relocations with provenance, validate entry mappings, derive the resident router and `CS:[0x066A]` constraints, determine whether the provider mapping is unique, emit a deterministic report, document findings, verify, and record the task.
