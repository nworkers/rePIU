# Glide clip window ABI 역추적 작업 지시

1. live telemetry에 Glide gate ESP, return EIP, 여덟 stack dword를 추가합니다.
2. supervisor snapshot에서 해당 값을 출력합니다.
3. Win32 x86 Debug를 빌드하고 `grClipWindow`까지 실행합니다.
4. return EIP 직전 호출부와 인자 producer를 역어셈블합니다.
5. 인자 생성과 stack cleanup 중 원인을 판정합니다.
6. 분석·아키텍처·작업 로그를 갱신하고, 안전하게 확정 가능한 수정만 구현합니다.

# Glide Clip Window ABI Trace Work Order

Extend live telemetry with Glide gate ESP, return EIP, and eight stack dwords; print them in supervisor snapshots; build and run Win32 x86 Debug through `grClipWindow`; disassemble the caller and argument producers before the return EIP; distinguish argument generation from stack-cleanup failure; update analysis, architecture, and the work log; and implement only a safely validated correction.
