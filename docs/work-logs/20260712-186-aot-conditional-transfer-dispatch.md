# AOT 조건 분기 dispatcher 작업 로그

표준 short/near Jcc를 guest EFLAGS로 판정하는 일반 dispatcher를 구현해 differential trace에 branch 선택을 노출했습니다. 이후 기본 블록 fall-through 누락이 직접 원인으로 확인되어, 내부 해석 가능한 표준 Jcc는 native rel32 emission으로 되돌렸습니다. dispatcher는 지원하지 않는 branch의 fallback으로 유지됩니다.

# AOT Conditional Transfer Dispatcher Work Log

Implemented a generic dispatcher that evaluates standard short/near Jcc from guest EFLAGS and exposed branch choices in the differential trace. After identifying missing basic-block fallthrough as the root cause, internally resolved standard Jcc returned to native rel32 emission. The dispatcher remains the fallback for unsupported branches.
