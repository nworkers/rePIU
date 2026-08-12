# 실행 probe 레지스터 메모리 창 작업 로그

## 결과

* `REPIU_EXECUTION_PROBE_OFFSET` first-hit snapshot에 `EAX`, `EBX`, `ECX`, `EDX`, `ESI`,
  `EDI`, `EBP`가 가리키는 32바이트 메모리 창을 추가했습니다.
* `REPIU_EXECUTION_PROBE_MEMORY_OFFSET`으로 각 레지스터에 공통 양수 offset을 적용할 수
  있으며, overflow와 읽을 수 없는 guest 범위는 무효로 남깁니다.
* `pumpit8`의 libpng iCCP helper 진입·반환·inflate 정리 지점을 반복 측정했습니다.

## 확인 결과

1. 함수 진입 입력은 `Photoshop ICC profile\0\0 78 9C...`로 정상입니다.
2. inflate는 입력 2,592바이트를 모두 소비하고 3,144바이트 sRGB ICC profile을
   출력하지만 `Z_STREAM_END`가 아니라 `Z_OK`로 끝납니다.
3. helper는 null을 반환하고 원본 chunk buffer를 해제합니다.
4. 호출자는 null에 prefix `0x17`을 더한 주소를 `repne scasb`로 읽어 접근 위반을
   일으킵니다.
5. 특정 target 또는 executable 주소에 대한 동작 우회는 추가하지 않았습니다.

## 검증

* `cmake --build build/win32_x86_debug --config Debug --target repiu repiu_aot_probe`: 성공
* Debug `pumpit8`, probe `+0xE49F8`: 정상 iCCP prefix와 zlib header 확인
* Debug `pumpit8`, probe `+0xE5D01`: null 반환과 해제된 입력 buffer 확인
* Debug `pumpit8`, probe `+0xE4C4D`: 최종 z_stream 상태와 sRGB profile 출력 확인

# Execution Probe Register Memory Window Work Log

## Result

* Extended the `REPIU_EXECUTION_PROBE_OFFSET` first-hit snapshot with 32-byte windows addressed by
  `EAX`, `EBX`, `ECX`, `EDX`, `ESI`, `EDI`, and `EBP`.
* Added `REPIU_EXECUTION_PROBE_MEMORY_OFFSET` as a common positive register offset. Overflow and
  unreadable guest ranges remain invalid.
* Repeatedly measured the `pumpit8` libpng iCCP helper entry, return, and inflate cleanup sites.

## Findings

1. Entry input is an intact `Photoshop ICC profile\0\0 78 9C...` sequence.
2. Inflate consumes all 2,592 input bytes and produces a 3,144-byte sRGB ICC profile, but ends in
   `Z_OK` rather than `Z_STREAM_END`.
3. The helper returns null and releases the original chunk buffer.
4. The caller adds prefix `0x17` to null and faults when `repne scasb` reads that address.
5. No target-specific or executable-address behavior bypass was added.

## Verification

* `cmake --build build/win32_x86_debug --config Debug --target repiu repiu_aot_probe`: passed
* Debug `pumpit8`, probe `+0xE49F8`: confirmed the intact iCCP prefix and zlib header
* Debug `pumpit8`, probe `+0xE5D01`: confirmed null return and released input buffer
* Debug `pumpit8`, probe `+0xE4C4D`: confirmed final z_stream state and sRGB profile output
