# 장시간 실행 관찰 작업 지시

1. loader 실행 제한 환경 계약을 추가한다.
2. supervisor가 종료 여유를 제외한 자식 제한을 전달하도록 한다.
3. Win32 x86 Debug 빌드와 기본 실행을 검증한다.
4. supervisor로 장시간 PIU 실행을 관찰하고 1초 기준선과 비교한다.
5. 분석 문서와 작업 로그를 갱신하고 커밋한다.

# Extended Execution Observation Work Order

Add the loader timeout environment contract, have the supervisor pass a child deadline with shutdown margin, validate the Win32 x86 Debug build and default execution, observe a longer PIU run and compare it with the one-second baseline, then update analysis and work-log documentation and commit.
