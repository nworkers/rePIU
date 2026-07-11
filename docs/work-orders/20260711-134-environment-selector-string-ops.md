# DOS environment selector와 LODSB 작업 지시

1. PSP `ES:[002Ch]` 반환을 `002Ch`로 복원한다.
2. software DS 기반 `LODSB` handler를 두 dispatch 경로에 연결한다.
3. Win32 x86 빌드와 PIU 실행 frontier로 검증하고 문서화한다.

# DOS Environment Selector and LODSB Work Order

Restore selector `002Ch`, add software-DS `LODSB` handling to both dispatch paths, then build and observe PIU.
