# AOT native return continuation 작업 로그

prototype은 `C3/C2`를 native로 실행하고 call stack에 cache return 주소를 넣었습니다. PIU 실행 초기에 `ntdll` access violation이 발생했습니다. guest 및 cache return 주소가 같은 stack에 공존하는 현재 HLE 경로에서는 안전하지 않으므로 구현을 되돌렸습니다.

# AOT Native Return Continuation Work Log

The prototype executed `C3/C2` natively and placed cache return addresses on the call stack. PIU raised an early `ntdll` access violation. Guest and cache return addresses coexist on the current HLE path, so the implementation was reverted as unsafe.
