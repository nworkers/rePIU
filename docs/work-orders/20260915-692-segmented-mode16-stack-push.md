# Task 692 작업 지시

현재 계속 진행 요청에 따라 [설계](../design/20260915-692-segmented-mode16-stack-push.md)를 구현합니다.

1. 공용 stack access policy와 mode16 PUSH adapter를 분리해 추가합니다.
2. shared HLE dispatch에 adapter를 연결하고 거부 시 기존 PUSH로 fallback하지 않습니다.
3. 공용 probe와 Linux 빌드, 실제 게임 실행으로 검증합니다.
4. 기존 분석을 정정하고 새 실행 결과 및 미해결 범위를 기록한 뒤 커밋합니다.

## English

Implement the linked design under the active request to continue.

1. Add separate shared stack access policy and mode16 PUSH adapter.
2. Connect shared HLE dispatch without falling back after a rejected PUSH.
3. Verify shared probes, Linux builds and live game execution.
4. Correct earlier analysis, record results and remaining gaps, then commit.
