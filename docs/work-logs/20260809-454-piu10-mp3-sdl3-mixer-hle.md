# 20260809-454 PIU10 MP3 SDL3_mixer HLE 작업 로그 / PIU10 MP3 SDL3_mixer HLE Work Log

설계: [20260809-454-piu10-mp3-sdl3-mixer-hle.md](../design/20260809-454-piu10-mp3-sdl3-mixer-hle.md)

작업 지시: [20260809-454-piu10-mp3-sdl3-mixer-hle.md](../work-orders/20260809-454-piu10-mp3-sdl3-mixer-hle.md)

## 한국어

### 결과

- `Piu10IsaBoard`가 8-bit `0x02DA` access를 처리하고 목적지 `0x008`의 low byte를
  MP3 sink로 전달하도록 확장했습니다. 16-bit data write도 같은 low-byte 규칙을 사용합니다.
- Win32 port adapter는 PIU10의 8-bit와 16-bit access를 폭별 API로 전달합니다. 32-bit
  access와 unavailable board는 기존처럼 fail-closed입니다.
- `Piu10Mp3AudioOut`을 별도 Win32 backend로 추가했습니다. 연속 guest byte stream을
  30 ms idle 경계로 묶고 SDL3_mixer 3.2.0의 `dr_mp3`로 predecode한 뒤 전용 track에서
  재생합니다. 4 byte보다 짧은 전송은 MPEG frame header가 될 수 없어 무시합니다.
- SDL3_mixer는 정적으로 고정했고 `mp3_drmp3`만 켰습니다. mpg123과 다른 codec은 모두
  비활성화했으며 zlib/MIT-0 license provenance를 기록했습니다.
- backend는 기존 `enable_piu10_isa_board` setup 안에서만 열리고 공용 board sink에
  연결되므로 pumpito/pumpitc/pumpitpc/pumpite에만 적용됩니다.

### 검증

1. `cmd /c scripts\build_win32_x86.bat`: 전체 Win32 x86 Debug 빌드 성공. CMake가
   `enabled: mp3_drmp3`와 나머지 decoder 비활성화를 출력했습니다.
2. `repiu_aot_probe.exe MASTER\PIU_1ST\PIU\PIU.EXE`: 전체 probe suite 성공.
   PIU10 probe는 byte `0xA5`와 word low byte `0xC3`가 sink에 순서대로 도착함을 포함합니다.
3. `REPIU_EXECUTION_TIMEOUT_MS=50000 repiu.exe pumpito`: 예외 없이 제한 시간까지 실행,
   port I/O input/output/handled/unhandled=`112955/264794/377749/0`.
4. 같은 실행에서 50,585-byte guest stream에 대해
   `decoded and started playback`을 확인했습니다. 초기 2-byte 전송 두 번은 실행을 막지
   않았고 후속 보정에서 decoder 호출 전에 제외했습니다.

### 남은 확인

현재 framing은 알려진 end-of-stream register가 없어 30 ms idle을 사용합니다. 느린 host에서
곡이 여러 segment로 나뉘는지와 실제 전체 곡의 연속 재생, frame-sync timing은 장시간 청취로
추가 확인할 항목입니다.

## English

### Result

- Extended `Piu10IsaBoard` to accept eight-bit `0x02DA` accesses and forward the low byte at
  destination `0x008` to an MP3 sink. Sixteen-bit data writes follow the same low-byte rule.
- The Win32 port adapter now routes eight-bit and sixteen-bit PIU10 accesses through
  width-specific APIs. Thirty-two-bit accesses and unavailable boards remain fail-closed.
- Added the dedicated Win32 `Piu10Mp3AudioOut` backend. It groups contiguous guest bytes at a
  30 ms idle boundary, predecodes through SDL3_mixer 3.2.0 `dr_mp3`, and plays on a dedicated
  track. Transfers shorter than four bytes cannot hold an MPEG frame header and are ignored.
- Pinned static SDL3_mixer with only `mp3_drmp3` enabled. mpg123 and every unrelated codec are
  disabled, with zlib/MIT-0 provenance recorded.
- The backend opens and connects to the platform-neutral board only inside the existing
  `enable_piu10_isa_board` setup, limiting it to pumpito/pumpitc/pumpitpc/pumpite.

### Verification

1. `cmd /c scripts\build_win32_x86.bat`: the full Win32 x86 Debug build passed. CMake reported
   `enabled: mp3_drmp3` and every other decoder disabled.
2. `repiu_aot_probe.exe MASTER\PIU_1ST\PIU\PIU.EXE`: the complete probe suite passed. The
   PIU10 probe now includes byte `0xA5` and word low byte `0xC3` arriving at the sink in order.
3. `REPIU_EXECUTION_TIMEOUT_MS=50000 repiu.exe pumpito`: ran to the time limit without an
   exception, with port I/O input/output/handled/unhandled=`112955/264794/377749/0`.
4. The same run reported `decoded and started playback` for a 50,585-byte guest stream. Two
   earlier two-byte transfers did not stop execution and are now filtered before decoding.

### Remaining Verification

Framing currently uses a 30 ms idle interval because no end-of-stream register is known.
Long listening should still verify whether a slow host splits a song into multiple segments,
continuous full-song playback, and precise frame-sync timing.
