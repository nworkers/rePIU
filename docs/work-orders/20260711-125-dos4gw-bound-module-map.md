# DOS4GW 결합 모듈·segment map 복원 작업 지시

1. DOS4GW MZ header와 overlay/bound-module 경계를 계산한다.
2. `LINEXE.EXP`, `D32_KERNEL` 등 알려진 module 이름과 directory record를 대응시킨다.
3. segment record에서 file base, size, logical base/IP를 복원한다.
4. `AH=FFh -> service index 0` jump target을 올바른 segment에 매핑한다.
5. provider 반환 `AL`, `GS`, flag와 private environment root 생성 코드를 기록한다.
6. 분석 문서와 작업 로그를 갱신하고 커밋한다.

# DOS4GW Bound-Module and Segment-Map Recovery Work Order

Calculate the DOS4GW MZ and overlay boundaries, correlate known module names such as `LINEXE.EXP` and `D32_KERNEL` with directory records, recover segment file bases, sizes and logical addresses, map the `AH=FFh` service-zero target to the correct segment, record provider `AL`, `GS`, flags and private-environment root construction, then update analysis and work-log documentation and commit.
