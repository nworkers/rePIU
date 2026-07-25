# 20260726-300 작업 지시: guest instruction probe fail-closed

설계: [docs/design/20260726-300-guest-instruction-probe-fail-closed.md](../design/20260726-300-guest-instruction-probe-fail-closed.md)

## 한국어

1. `DispatchGuestException`의 HLE decoder 진입 전에 15바이트 guest decode window를
   확인하고 실패 시 원래 예외를 캡처·복구합니다.
2. `DispatchGuestHleHandlers`에도 같은 진입 guard를 추가합니다.
3. 네 `HandleTraced*Interrupt*` 함수에 2바이트 guest-range guard를 추가합니다.
4. Win32 Debug 빌드와 정적 검증을 수행합니다.
5. 사용자 `repiu_log.txt`는 수정하거나 commit하지 않습니다.

## English

Add a 15-byte fail-closed decode gate to both guest instruction dispatch
entry points, add independent two-byte guards to the four traced interrupt
handlers, build Win32 Debug, and statically verify the guards. Preserve the
user-owned log unchanged.
