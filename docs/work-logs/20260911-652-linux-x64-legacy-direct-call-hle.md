# 20260911-652 작업 로그: Linux x64 legacy fallback direct CALL HLE

설계: [20260911-652 설계](../design/20260911-652-linux-x64-legacy-direct-call-hle.md)  
작업 지시: [20260911-652 작업 지시](../work-orders/20260911-652-linux-x64-legacy-direct-call-hle.md)  
분석: [linux-port-frontier 3.89](../analysis/linux-port-frontier.md)

## 한국어

### 결과

1. 잘못 연결됐던 두 prologue를 분리했습니다. `0x010F920C`는 일곱 PUSH 뒤 CMP로
   이어지고, `0x010F1D74..0x010F1E56`이 독립적인 allocator frame입니다.
2. 정적 xref와 실제 address watch로 allocator 진입이
   `0x010F9258 CALL 0x010F1D74`임을 확인했습니다.
3. x64 legacy fallback의 `E8 rel32`를 shared HLE에서 처리하여 guest 반환 주소,
   ESP/EIP와 기존 AOT call frame을 보존했습니다.
4. 실제 실행에서 `0x0158CC50 = 0x010F925D` write와
   `0x010F1E56 RET -> 0x010F925D`를 확인했습니다. 기존 zero-return frontier는
   해소됐습니다.

### 검증

* Linux x64 Debug `repiu`와 `repiu_core_probe` 빌드: 통과
* 전체 core probe: `27/27`, failures `0`
* 합성 legacy direct CALL 및 arena 밖 target 거부: 통과
* 실제 `pumpit2a`: 올바른 반환 target까지 전진

게임은 아직 정상 실행되지 않습니다. 새 frontier는 `0x010F925D` 재번역이
`0x010F928B` segment-override coverage에서 거절되는 것입니다.

## English

### Result

1. Separated two prologues that had been incorrectly connected:
   `0x010F920C` has seven PUSH instructions followed by CMP, while
   `0x010F1D74..0x010F1E56` is an independent allocator frame.
2. Static xrefs and a live address watch identified the allocator entry as
   `0x010F9258 CALL 0x010F1D74`.
3. Added shared HLE for `E8 rel32` during x64 legacy fallback, preserving the
   guest return address, ESP/EIP, and existing AOT call frame.
4. The real run confirmed the write `0x0158CC50 = 0x010F925D` and
   `0x010F1E56 RET -> 0x010F925D`. The former zero-return frontier is resolved.

### Verification

* Linux x64 Debug `repiu` and `repiu_core_probe` build: passed
* Complete core probe: 27/27, zero failures
* Synthetic legacy direct CALL and outside-arena target refusal: passed
* Real `pumpit2a`: advanced to the correct return target

The game still does not run normally. The new frontier is rejection of
`0x010F925D` retranslation at segment-override coverage address `0x010F928B`.
