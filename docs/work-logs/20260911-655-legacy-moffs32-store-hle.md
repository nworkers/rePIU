# 20260911-655 작업 로그: legacy fallback moffs32 store HLE

설계: [20260911-655 설계](../design/20260911-655-legacy-moffs32-store-hle.md)  
작업 지시: [20260911-655 작업 지시](../work-orders/20260911-655-legacy-moffs32-store-hle.md)  
분석: [linux-port-frontier 3.92](../analysis/linux-port-frontier.md)

## 한국어

### 결과

1. 공용 memory-store HLE에 prefix 없는 `A3 disp32` 형식을 추가했습니다.
2. destination은 little-endian 32-bit offset, value는 guest EAX, width는 4,
   instruction size는 5로 처리합니다.
3. 기존 guest writable 검사와 `WriteGuestUInt32`를 사용하므로 provenance와 범위 거부
   계약을 유지하며 MOV EFLAGS도 보존합니다.
4. 실제 `pumpit2a`는 `0x010F927C` write를 완료하고 이전 SIGSEGV를 넘어 훨씬 뒤의
   반환 경계까지 진행했습니다.

### 검증

* Linux x64 Debug `repiu`와 `repiu_core_probe` 빌드: 통과
* 합성 `A3`: write 값, EIP `+5`, EFLAGS 보존, arena 밖 거부 통과
* 전체 core probe: `27/27`, failures `0`
* 실제 write: `[0x011A6698] = 0x0158CCD0`, source `0x010F927C`

### 남은 경계

게임은 아직 정상 실행되지 않습니다. 새 frontier는
`0x010F1D71 RET -> 0x0103B1DB`이며, 동적 CFG coverage는 `0x010F44E6`에서
거절됩니다. 다음 작업은 이 반환 target의 첫 명령과 coverage 거절 경로를 분석해 제한적
bridge 또는 HLE가 안전한지 판정하는 것입니다.

## English

### Result

1. Added unprefixed `A3 disp32` to shared memory-store HLE.
2. It decodes a little-endian 32-bit destination, guest EAX source, four-byte
   width, and five-byte instruction size.
3. The existing guest-writable check and `WriteGuestUInt32` preserve provenance
   and range refusal, while MOV EFLAGS remain unchanged.
4. Real `pumpit2a` completed the `0x010F927C` write and progressed far beyond
   the former SIGSEGV to a later return boundary.

### Verification

* Linux x64 Debug `repiu` and `repiu_core_probe` build: passed
* Synthetic `A3`: value, EIP `+5`, EFLAGS preservation, and range refusal passed
* Complete core probe: 27/27, zero failures
* Real write: `[0x011A6698] = 0x0158CCD0`, source `0x010F927C`

### Remaining frontier

The game still does not run normally. The new frontier is
`0x010F1D71 RET -> 0x0103B1DB`, whose dynamic CFG coverage is rejected at
`0x010F44E6`. The next task must analyze the return target's first instruction
and the coverage rejection path before deciding whether another guarded bridge
or HLE operation is safe.
