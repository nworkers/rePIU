# LINEXE HLE call-gate ABI 작업 지시

1. PIU LE object/page map으로 DLL loader 함수들을 원본 file offset에 연결한다.
2. 네 export pointer가 저장되는 global을 확정한다.
3. 각 global의 모든 read와 indirect call site를 열거한다.
4. 호출 전후 data flow로 argument, cleanup, return contract를 복원한다.
5. call-gate trap encoding, synthetic selector/image와 dispatcher ABI를 설계한다.
6. 근거가 충분한 export부터 구현하고 private environment와 함께 활성화한다.
7. build/runtime 검증, 분석 문서, 작업 로그와 커밋을 남긴다.

진행 메모: 1~5단계와 플랫폼 공용 합성 게이트 계획을 완료했습니다. 6단계 활성화 전 runtime arena에 LINEXE 전용 예약 영역을 명시적으로 추가합니다.

# LINEXE HLE Call-Gate ABI Work Order

Map PIU loader functions to source offsets, identify export-pointer globals and all uses, recover call ABI and return contracts, design trap-backed synthetic pointers and dispatcher state, implement evidence-backed exports with atomic environment activation, verify builds/runtime, document, and commit.
