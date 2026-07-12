# Zydis verified region 작업 지시 / Work Order

## 한국어

1. Zydis v4.1.1과 Zycore MIT 라이선스를 pinned amalgamation과 함께 vendoring한다.
2. decoder-only CMake target과 Win32 fast-path linkage를 추가한다.
3. 자체 instruction-length decoder를 Zydis adapter로 교체한다.
4. 민감 category/attribute와 indirect/far control-flow를 fail-closed로 거부한다.
5. synthetic decode/CFG 검증과 Win32 x86 Debug PIU 실행을 수행한다.
6. 라이선스, architecture, analysis 및 작업 로그를 갱신한다.

## English

1. Vendor pinned Zydis v4.1.1 amalgamation with Zydis and Zycore MIT licenses.
2. Add a decoder-only CMake target and Win32 fast-path linkage.
3. Replace the in-house instruction-length decoder with a Zydis adapter.
4. Fail closed on sensitive categories/attributes and indirect/far control flow.
5. Run synthetic decode/CFG verification and Win32 x86 Debug PIU execution.
6. Update licenses, architecture, analysis, and the work log.
