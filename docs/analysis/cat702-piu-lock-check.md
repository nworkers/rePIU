# CAT702 PIU 보안 검사 분석 / CAT702 PIU Lock Check Analysis

## 한국어

### 확인됨

- `pumpitpc.cat702` 변환 데이터는 `F0 1C FE 03 81 40 38 F8`이며 CRC32는
  `d389a56b`입니다.
- 실제 challenge `D0 AA C9 F8 96 4C D0 DE B6 0B`에 대한 예상 response는
  `7D 77 FE EA 8B 7A 55 7D 5F 1E`입니다.
- 게스트는 challenge를 LSB-first 및 반전된 데이터 선으로 전송하고, 응답을
  MSB-first로 누적한 뒤 두 번 더 clock하고 인접 바이트를 3-bit 정렬한 다음 각
  바이트의 bit order를 뒤집습니다. 이 전 과정을 적용하면 기존 CAT702 HLE 모델이
  예상 response와 정확히 일치합니다.
- 검증 함수 `0x01019818`, 직렬 함수 `0x010195BE`, 목적지 helper `0x01018AB2`가
  실행되며 helper는 공용 `outpw` 래퍼 `0x010EC743`의 `OUT DX,AX`를 사용합니다.
- 초기 `0x2AC`, `0x2A6` 출력이 무시 대상 경로에서 래퍼의 `OUT` 명령을 NOP로
  바꿨습니다. 이후 같은 래퍼 호출은 계속되지만 PIU10 포트 `0x2D4`, `0x2D6`,
  `0x2DA` 출력은 실행될 수 없었습니다.
- Lock Error 화면 뒤의 DOS `INT 21h AH=08` 미지원 종료는 보안 검사 실패에 따른
  키 대기의 2차 증상입니다.

### 결론

실패 원인은 CAT702 ROM, 변환, bit order 또는 AOT word 출력 분류가 아니라, 동적 DX
포트 래퍼를 무시 대상 출력 하나만 보고 영구 패치한 것입니다. 무시·유예 출력에서는
현재 EIP만 전진하여 해당 트랜잭션만 버리고 공용 래퍼 코드는 보존해야 합니다.

### 미확정

현재 CAT702 모델과 실제 검사 벡터 사이에는 미확정 차이가 없습니다. 다른 실행 파일이
추가 CAT702 명령 형식을 사용하는지는 별도의 호환성 검증 대상입니다.

## English

### Confirmed

- The `pumpitpc.cat702` transform is `F0 1C FE 03 81 40 38 F8`, CRC32 `d389a56b`.
- Challenge `D0 AA C9 F8 96 4C D0 DE B6 0B` expects response
  `7D 77 FE EA 8B 7A 55 7D 5F 1E`.
- The guest sends challenge bits LSB-first on an inverted data line, accumulates response bits
  MSB-first, clocks twice more, aligns adjacent bytes by three bits, and reverses each result
  byte. With the complete guest processing applied, the existing CAT702 HLE model matches the
  expected response exactly.
- Verifier `0x01019818`, serial routine `0x010195BE`, and destination helper `0x01018AB2`
  execute. The helper uses `OUT DX,AX` in shared `outpw` wrapper `0x010EC743`.
- Initial ignored writes to `0x2AC` and `0x2A6` NOP-patched the wrapper's `OUT` instruction.
  Calls to the wrapper continued, but later PIU10 writes to `0x2D4`, `0x2D6`, and `0x2DA`
  could no longer execute.
- Unsupported DOS `INT 21h AH=08` termination after the Lock Error screen is secondary key-wait
  behavior following the failed security check.

### Conclusion

The failure is not in the CAT702 ROM, transform, bit order, or AOT word-output classification.
It is caused by permanently patching a dynamic DX-port wrapper after one ignored output. Ignored
or deferred outputs must advance only the current EIP, discarding that transaction while keeping
the shared wrapper intact.

### Unresolved

There is no unresolved difference between the current CAT702 model and the observed check vector.
Compatibility with additional CAT702 command forms in other executables remains separate work.
