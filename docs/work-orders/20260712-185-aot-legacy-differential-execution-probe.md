# AOT/legacy differential execution probe 작업 지시

1. probe offset 환경 변수를 파싱합니다.
2. ThreadContext/attempt에 최초 hit snapshot을 추가합니다.
3. legacy single-step과 AOT guest remap 지점에서 동일 recorder를 호출합니다.
4. PIU return offset에서 두 backend를 비교합니다.
5. 차이가 발생한 call-site 또는 함수 내부로 probe를 역추적합니다.
6. 최초 divergence와 수정 방향을 문서화·검증·커밋합니다.

# AOT/Legacy Differential Execution Probe Work Order

Parse a probe offset, capture an identical first-hit snapshot in legacy and AOT, compare the PIU return site, trace backward to the first divergence, document and verify the correction, and commit.
