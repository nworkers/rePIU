# 작업 로그: 호출 래퍼형 PIU10 MP3 feeder 일괄 전송

## 결과

`pumpitpc`의 byte-output 래퍼를 통과하는 MP3 feeder를 고정 주소나 target profile 조건 없이
frame-tail batch에 연결했습니다. 기존 direct matcher는 변경 없이 우선 적용되며, 실패할
때만 guest stack과 call/return 관계까지 검증하는 wrapped matcher를 시도합니다.

## 구현 내용

- frame batch API에 현재 guest ESP를 전달했습니다.
- arena와 AOT fast path가 각 실행 문맥의 정확한 guest ESP를 제공합니다.
- wrapped matcher는 register 보존 wrapper, `[ESP+4]` 반환 주소, 상대 call 대상, source
  cursor/frame count/`ECX` 갱신, frame target 비교, backward loop 및 service 조건을 검증합니다.
- synthetic probe에 relocated wrapped 성공과 손상된 반환 주소 fail-closed arm을 추가했습니다.
- audit plan도 compressed inflight의 `0xE00` `DEMAND` 여유에서 제한하여 scalar status 반환
  경계를 넘지 않게 했습니다.

## 검증

- Win32 x86 Debug 전체 빌드: 성공
- Win32 x86 Release 전체 빌드: 성공
- Release `repiu_aot_probe pumpitpc/PIU/PIU.EXE`: 성공
  - `wrapped=true`, `variant=true`, `fail-closed=true`
- Release `pumpitpc`, audit 35초: 3,700개 연속 segment 통과, mismatch 없음
- Release `pumpitpc`, 일반 25초:
  - playback 시작: 2,902 guest byte
  - received/batched: 140,795 / 136,595 byte (약 97.0%)
  - dropped/starved: 0 / 0

빌드에는 기존 MSVC C4819 및 probe LNK4217 경고가 있었지만 새 오류는 없었습니다. 제한
실행은 설정한 timeout으로 정상 정리됐습니다.

---

# Work log: wrapped-call PIU10 MP3 feeder batching

## Result

The `pumpitpc` MP3 feeder that reaches its byte-output wrapper now uses frame-tail batching without
a fixed address or target-profile condition. The unchanged direct matcher runs first; only its
failure invokes the wrapped matcher, which additionally validates guest-stack and call/return
relationships.

## Implementation

- Passed current guest ESP through the frame-batch API.
- Supplied exact guest ESP from both arena and AOT fast paths.
- Validated the register-preserving wrapper, `[ESP+4]` return address, relative call target, source
  cursor/frame count/`ECX` updates, frame-target comparison, backward loop, and service checks.
- Added a relocated wrapped success case and corrupted-return fail-closed arm to the synthetic probe.
- Limited audit plans by compressed-inflight headroom to the logical `0xE00` `DEMAND` boundary so
  they cannot cross a scalar status-return boundary.

## Verification

- Complete Win32 x86 Debug build: passed
- Complete Win32 x86 Release build: passed
- Release `repiu_aot_probe pumpitpc/PIU/PIU.EXE`: passed
  - `wrapped=true`, `variant=true`, `fail-closed=true`
- Release `pumpitpc`, 35-second audit: 3,700 consecutive segments, no mismatch
- Release `pumpitpc`, normal 25-second run:
  - playback began after 2,902 guest bytes
  - received/batched: 140,795 / 136,595 bytes (about 97.0%)
  - dropped/starved: 0 / 0

The build retained existing MSVC C4819 and probe LNK4217 warnings but introduced no errors. Each
bounded run shut down through its configured timeout.
