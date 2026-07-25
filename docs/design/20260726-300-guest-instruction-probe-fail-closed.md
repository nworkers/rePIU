# 20260726-300 guest instruction probe fail-closed

## 한국어

### 배경

Task 299 입력 로그의 host AV `0x10238727`은 PDB상
`HandleTracedDosInterrupt21`의 `instruction[0]`입니다. 당시 guest EIP는 이미
`0x43F00000`으로 비정상이었고 해당 주소는 미할당 영역이었습니다. 따라서 이것은
원래 guest RET 실패를 가린 2차 예외입니다. Task 296 작업 로그에서도 같은 함수의
무가드 probe가 다음 frontier로 예측됐습니다.

한 handler만 고치면 dispatcher가 다음 segment/memory decoder를 호출해 같은 EIP를
다시 읽을 수 있으므로 dispatch 경계와 재사용 공용 chain을 함께 막아야 합니다.

### 설계

1. guest-stack VEH에서 AOT EIP→guest EIP 변환 후 최대 x86 instruction 길이
   15바이트가 guest runtime에 없으면 HLE decoder를 호출하지 않습니다.
2. 기존 최종 unhandled 경로와 같은 방식으로 원래 예외를 캡처하고 host로
   복구합니다.
3. `DispatchGuestHleHandlers`도 진입 시 같은 15바이트 조건으로 false를 반환합니다.
4. 네 traced interrupt handler는 독립 호출에도 안전하도록 필요한 2바이트를 직접
   검증합니다.
5. 주소를 복구하거나 추측하지 않으며 원래 exception code/fault VA를 보존합니다.

### 검증

- Win32 x86 Debug loader 빌드
- 정적 검색으로 네 handler guard와 두 dispatch guard 확인
- `git diff --check`

---

## English

The host AV resolves to the unguarded opcode probe in
`HandleTracedDosInterrupt21`, after guest EIP was already the unallocated
`0x43F00000`. Guarding only that function would merely pass the same address to
the next decoder.

After AOT-to-guest EIP translation, guest-stack VEH dispatch requires a full
15-byte x86 decode window before calling any HLE decoder; otherwise it captures
the original exception and recovers through the existing fail-closed path.
The reusable handler chain applies the same gate, and all four traced interrupt
handlers independently validate their two-byte opcode. No guessed EIP recovery
is introduced.
