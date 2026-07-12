# AOT indirect transfer dispatcher 작업 지시

1. cache sentinel의 guest `FF /2`, `FF /4`를 분류합니다.
2. register 및 ModRM memory target을 계산합니다.
3. target map 또는 on-demand append를 수행합니다.
4. call guest fallthrough을 guest stack에 기록합니다.
5. dispatch telemetry와 마지막 source/target을 추가합니다.
6. PIU `aot-dynamic` frontier를 다시 관찰합니다.
7. 문서·검증·작업 로그를 갱신하고 커밋합니다.

# AOT Indirect Transfer Dispatcher Work Order

Classify and resolve near indirect call/jump sentinels, map or append their targets, preserve guest call returns, add telemetry, observe the new PIU frontier, document and verify the result, and commit.
