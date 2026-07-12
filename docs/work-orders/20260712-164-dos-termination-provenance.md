# DOS 종료 provenance 작업 지시 / Work Order

## 한국어

1. 종료 AX/EIP/ESP와 stack 128 dword를 실행 결과에 추가한다.
2. 두 `AH=4Ch` 처리 경로가 같은 capture 함수를 사용하도록 한다.
3. loader에서 stack을 출력한다.
4. PIU 실행 후 original image return address를 정적으로 역매핑한다.

## English

1. Add termination AX/EIP/ESP and 128 stack dwords to execution results.
2. Use one capture function from both `AH=4Ch` paths.
3. Print the stack from the loader.
4. Run PIU and statically map original-image return addresses.
