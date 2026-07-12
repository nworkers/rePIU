# First-chance guard-page 진단 작업 지시 / Work Order

1. 선택형 Win32 debug event loop를 supervisor에 추가합니다.
2. guard-page thread context, exception parameters, instruction bytes와 page 정보를 출력합니다.
3. 초기 breakpoint와 guest exception의 continue status를 분리합니다.
4. 빌드 후 짧은 `debug-exceptions` 실행으로 `0x80000001` provenance를 확인합니다.
5. 원인에 맞는 최소 수정 후 일반 supervisor 실행을 재검증합니다.

## English

Add an optional debug loop, capture first-chance guard-page evidence without consuming guest exceptions, verify it in a short run, then make the smallest evidence-based correction and retest normal supervisor mode.
