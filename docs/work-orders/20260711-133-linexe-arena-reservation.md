# LINEXE 전용 arena 예약 작업 지시

1. HLE base와 arena end로부터 세 전용 페이지와 allocator 시작점을 계산한다.
2. overflow, 정렬, arena 포함 여부를 검증한다.
3. placement와 실행 context에 layout을 전달한다.
4. selector descriptor와 private environment를 세 페이지에 설치한다.
5. gate 예외 dispatch와 `AX=FF00h`를 원자적으로 활성화한다.
6. Win32 x86 빌드와 실행 관찰로 검증한다.

# LINEXE-Owned Arena Reservation Work Order

Calculate and validate three owned HLE pages plus the dynamic allocator base, propagate the layout through placement and execution, install descriptors and private data, atomically enable gate dispatch and DOS/4GW identification, then verify the Win32 x86 build and runtime.
