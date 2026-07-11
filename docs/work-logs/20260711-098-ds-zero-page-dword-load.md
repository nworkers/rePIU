# DS zero-page dword load 작업 로그

## 결과

일반 `8B /r` memory load가 실제 arena와 shadow memory에서 값을 찾지 못했을 때, guest `DS`가 활성화되어 있고 source dword 전체가 첫 4 KiB 안에 있는 경우 zero-backed DOS low-memory 값을 반환하도록 구현했습니다. guest 주소 0에 relocated image base를 더하지 않습니다.

## 관찰

반복 실행에서 `ESI=0`인 relocated base + `0x000F7A71`의 `8B 16`을 통과했습니다. 새 blocker는 relocated base + `0x000F7BAD`의 `03 07`, 즉 `add eax, dword ptr [edi]`입니다. 이때 `EDI=0x026E49C4`로 기존 shadow allocator metadata 범위에 있으므로 다음 작업은 `03 /r`의 shadow-memory source 처리입니다.

같은 기존 명령에서 `ESI=0xFF000000`이 관찰된 실행은 처리하지 않고 그대로 fault로 남았습니다. 따라서 zero-page 조건이 임의의 고주소 접근을 흡수하지 않음을 확인했습니다.

처리 이후 즉시 다음 fault가 발생하지 않는 실행은 충분한 store/read 진척과 `stage.cfg` open 뒤 diagnostic quiet timeout으로 끝납니다. 회귀 테스트는 유효한 후속 exception과 이 진척 후 timeout을 각각 검증합니다.

## 검증

* Win32 x86 Debug 빌드: 성공
* `dos4gw_hello`: 정상 반환
* `piu_1st` 반복 실행: `0x000F7A71` 통과 및 `0x000F7BAD` 관찰
* 전체 테스트: 성공

# DS Zero-Page Dword Load Work Log

## Result

Implemented a zero-backed DOS low-memory fallback for generic `8B /r` loads when real-arena and shadow-memory reads miss, guest `DS` is active, and the complete source dword lies inside the first 4 KiB. The handler does not add the relocated image base to guest address zero.

## Observation

Repeated execution passed `8B 16` at relocated base + `0x000F7A71` when `ESI=0`. The new blocker is `03 07` at relocated base + `0x000F7BAD`, or `add eax, dword ptr [edi]`. With `EDI=0x026E49C4` inside the existing shadow allocator-metadata range, the next task is a shadow-memory source path for `03 /r`.

Runs that reached the same original instruction with `ESI=0xFF000000` remained unhandled and faulted normally, confirming that the zero-page condition does not absorb arbitrary high-address accesses.

Runs without an immediate following fault end through the diagnostic quiet timeout after sufficient store/read progress and opening `stage.cfg`. The regression test validates both a recognized following exception and this post-progress timeout.

## Verification

* Win32 x86 Debug build: passed
* `dos4gw_hello`: returned normally
* Repeated `piu_1st` execution: passed `0x000F7A71` and observed `0x000F7BAD`
* Full test set: passed
