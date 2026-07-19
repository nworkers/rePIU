# Dispatch zero-EIP fail-closed design

`DispatchGuestException`은 zero EIP context를 instruction/AOT decoder에 전달하지 않고 host
recovery로 넘겨 원래 예외 context를 보존합니다.

`DispatchGuestException` must not pass a zero-EIP context to instruction or AOT decoders; it
recovers to the host while preserving the original exception context.
