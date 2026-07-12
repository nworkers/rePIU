# 선택 가능한 AOT 실행 backend 작업 지시

181-A 준비 단계:

1. return을 외부 dispatcher 경계로 emit합니다.
2. Win32 RX code-cache placement와 양방향 lookup을 독립 파일로 구현합니다.

181-B 실행 연결 단계:

3. 실행 trampoline에 선택적 AOT placement를 전달합니다.
4. cache breakpoint를 guest HLE/간접 경계로 변환하고 cache 재진입을 구현합니다.
5. `REPIU_EXECUTION_BACKEND` 선택과 backend telemetry를 loader에 추가합니다.
6. legacy 기본 경로의 빌드·실행이 유지되는지 검증합니다.
7. AOT opt-in 실행의 최초 진행 frontier와 성능 계수를 측정합니다.
8. analysis, architecture, 작업 로그를 갱신하고 커밋합니다.

# Selectable AOT Execution Backend Work Order

Rewrite returns as dispatch boundaries, add isolated Win32 RX cache placement and lookup, connect an optional AOT placement to the trampoline, bridge cache exceptions through the existing guest HLE path, expose backend selection and telemetry, verify the unchanged legacy default, observe the opt-in AOT frontier, document results, and commit.
