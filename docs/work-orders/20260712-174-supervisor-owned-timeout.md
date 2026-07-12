# Supervisor 전담 timeout 작업 지시 / Work Order

1. loader timeout 환경 값 `0`을 `INFINITE`로 정의합니다.
2. `INFINITE`에서는 quiet timeout도 적용하지 않습니다.
3. supervisor는 child timeout으로 항상 `0`을 전달합니다.
4. 짧은 실행에서 supervisor가 exit 124로 process 전체를 종료하고 잔류 process가 없는지 확인합니다.
5. 장시간 실행에서 이전 369초 teardown 접근 위반이 사라지는지 확인합니다.

## English

Define environment value zero as infinite loader execution, disable quiet timeout in that mode, make the supervisor pass zero, and verify supervisor-owned process termination without residual processes or the former loader teardown crash.
