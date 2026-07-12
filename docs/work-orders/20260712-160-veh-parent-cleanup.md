# Guest VEH parent cleanup 작업 지시

1. `ThreadContext`에 VEH handle을 보존합니다.
2. guest worker의 직접 handler 제거를 없앱니다.
3. parent wait/join 이후 handler를 제거하는 공통 cleanup을 추가합니다.
4. 정상 DOS terminate와 timeout 경로를 모두 검증합니다.
5. 분석·아키텍처·작업 로그를 갱신하고 커밋합니다.

# Guest VEH Parent Cleanup Work Order

Store the VEH handle in `ThreadContext`; remove direct cleanup from the guest worker; add parent-side cleanup after wait/join; verify normal DOS termination and timeout paths; update analysis, architecture, and the work log; and commit.
