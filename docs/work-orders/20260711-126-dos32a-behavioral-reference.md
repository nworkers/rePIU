# DOS/32A 동작 참고 명시 작업 지시

1. DOS/32A 공식 저장소에서 `INT 21h AX=FF00h` 입력·출력 계약을 확인한다.
2. `LINEXE_*` private module 구조의 구현 여부를 저장소 전체에서 확인한다.
3. 설계와 분석 문서에 동작 참고 범위, 외부 링크, 코드 미사용 원칙을 기록한다.
4. Win32 HLE 코드에 참고 출처와 독립 구현 경계를 주석 및 상수로 명시한다.
5. 문서 링크와 빌드를 검증하고 작업 로그를 작성한다.

# DOS/32A Behavioral Reference Attribution Work Order

Verify the official `INT 21h AX=FF00h` contract and search for evidence of the `LINEXE_*` private module layout. Document the behavioral-reference-only boundary and official links, add source attribution and clean-room constants to the Win32 HLE, verify documentation links and the build, and record the results.
