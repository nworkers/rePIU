# Host VEH 예외 경계 작업 지시 / Host VEH Exception Boundary Work Order

## 한국어

1. WGL 초기화 중 발생한 `0x406D1388`의 주소 영역과 호출 시점을 확인한다.
2. host 주소에서 발생한 해당 예외만 Windows 예외 체인으로 전달한다.
3. Win32 x86 빌드와 실제 `piu_1st` 실행으로 다음 Glide frontier를 확인한다.

## English

1. Confirm the address domain and timing of `0x406D1388` during WGL initialization.
2. Pass only that exception from host addresses to the Windows exception chain.
3. Build Win32 x86 and run `piu_1st` to identify the next Glide frontier.
