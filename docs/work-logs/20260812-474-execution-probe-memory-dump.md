# 실행 probe 메모리 구간 dump 작업 로그

## 결과

* `REPIU_EXECUTION_PROBE_DUMP_*` 환경 변수 그룹으로 probe first-hit 시점의 guest 메모리
  구간을 파일로 보존하는 진단을 추가했습니다. base는 레지스터 이름 또는 절대 주소이며,
  선택적 indirect 역참조와 최대 1 MiB 복사를 지원합니다.
* 이 진단으로 `pumpit8` iCCP 종료의 원인 사슬을 끝까지 확정했습니다.

## 확인 결과

1. runtime `iCCP` chunk `0xA37` 바이트 전체를 추출해 host 독립 zlib decoder로 검증했습니다.
   compressed 필드 2,592바이트는 온전한 3,144바이트 sRGB ICC profile을 만들지만, deflate
   stream이 필드 마지막 바이트에서 끝나 adler32 4바이트가 **전부 없습니다**. 2,590바이트에서
   한 바이트만 줄여도 출력이 나오지 않으므로 결손량은 정확히 4바이트입니다.
2. PNG stream 본체를 png context `+0x0C` indirect dump로 확보했습니다. chunk 구조는
   IHDR / iCCP / gAMA / cHRM / IDAT가 모두 정합하고, PNG의 chunk length 필드 자체가
   `0xA37`입니다.
3. `iCCP` chunk의 CRC32는 저장값과 계산값이 모두 `0x8FC1BA08`으로 일치합니다. 따라서 chunk는
   작성된 그대로 메모리에 도달했으며, **rePIU의 파일 I/O·RES 복호화·PNG streaming에는 결함이
   없습니다.** 결손은 원본 자산 자체의 작성 시점 결함입니다.
4. 원본 코드의 호출 지점을 정적 역어셈블한 결과 `call` 다음에 null 검사가 없습니다.
   `lea ebx,[eax+edi]` 후 `repne scasb`가 linear `0x17`을 읽는 것은 원본의 결정적 동작입니다.
5. rePIU 측 결함은 `HandleGuestLowMemoryReadFault`가 `MOV`, `MOVZX`, `MOVSX`만 처리하고
   `repne scasb`를 stage 4로 거부하는 것입니다. 이미 존재하는 low memory 대행 facility의
   명령 집합이 좁아서 접근 위반이 처리되지 않습니다.
6. 특정 target 또는 executable 주소에 대한 동작 우회를 추가하지 않았습니다.

## 진행 중 관측

* `REPIU_EXECUTION_TIMEOUT_MS`를 설정하면 전체 상한과 함께 1초 무진척 정지 감지기가 같이
  켜집니다. 값을 주고 실행하면 iCCP 지점에 닿기 전에 70초 부근에서 종료되므로, 이 재현에는
  기본값(무제한)이 필요합니다.
* `BGA/083.DAT`는 `RES\0` version 3 container이며 payload 전체가 고엔트로피입니다. 파일에는
  PNG signature도 `iCCP` 문자열도 없어 원본 자산의 정적 추출이 불가능합니다.

## 검증

* `cmake --build build/win32_x86_debug --config Debug --target repiu`: 성공
* `cmake --build build/win32_x86_debug --config Debug --target repiu_aot_probe`: 성공
* Debug `pumpit8`, probe `+0xE49F8`, dump `0x0526BCF0` `0xA37`바이트: chunk 추출 성공
* Debug `pumpit8`, indirect dump `eax+0xC` 1 MiB: PNG stream 확보 및 CRC32 일치 확인
* 환경 변수 미설정 시 dump 경로가 비활성으로 남는 것을 같은 실행에서 확인

# Execution Probe Memory Range Dump Work Log

## Result

* Added a diagnostic that preserves a guest memory range to a file at the probe's first hit,
  configured by the `REPIU_EXECUTION_PROBE_DUMP_*` group. The base is a register name or an
  absolute address, with optional indirect dereference and up to 1 MiB copied.
* Used it to establish the complete cause chain of the `pumpit8` iCCP termination.

## Findings

1. Extracting all `0xA37` bytes of the runtime `iCCP` chunk and validating them with an independent
   host zlib decoder shows the 2,592-byte compressed field producing the intact 3,144-byte sRGB ICC
   profile, with the deflate stream ending on the field's last byte so that **all four adler32
   bytes are absent**. Removing even one byte from the 2,590 raw bytes yields no output, so the
   shortfall is exactly four bytes.
2. An indirect dump based at png context `+0x0C` captured the PNG stream body. IHDR, iCCP, gAMA,
   cHRM, and IDAT all align, and the PNG's own chunk length field reads `0xA37`.
3. The `iCCP` chunk's stored and computed CRC32 both equal `0x8FC1BA08`. The chunk therefore
   reached memory exactly as authored, so **rePIU's file I/O, RES decryption, and PNG streaming are
   all sound.** The shortfall is an authoring-time defect in the original asset.
4. Static disassembly of the original call site shows no null check after the `call`. The
   `lea ebx,[eax+edi]` followed by `repne scasb` reading linear `0x17` is deterministic original
   behavior.
5. The rePIU-side defect is that `HandleGuestLowMemoryReadFault` handles only `MOV`, `MOVZX`, and
   `MOVSX`, rejecting `repne scasb` at stage 4. The existing low-memory servicing facility covers
   too narrow an instruction set, so the access violation goes unhandled.
6. No target-specific or executable-address behavior bypass was added.

## Observations Along the Way

* Setting `REPIU_EXECUTION_TIMEOUT_MS` also arms a one-second no-progress stall detector alongside
  the overall bound. With a value set, the run ends near 70 seconds before reaching the iCCP site,
  so reproducing this requires the unlimited default.
* `BGA/083.DAT` is a `RES\0` version-3 container whose payload is entirely high-entropy. It
  contains neither a PNG signature nor an `iCCP` string, so the original asset cannot be extracted
  statically.

## Verification

* `cmake --build build/win32_x86_debug --config Debug --target repiu`: passed
* `cmake --build build/win32_x86_debug --config Debug --target repiu_aot_probe`: passed
* Debug `pumpit8`, probe `+0xE49F8`, dump of `0xA37` bytes at `0x0526BCF0`: chunk extracted
* Debug `pumpit8`, indirect 1 MiB dump at `eax+0xC`: PNG stream captured and CRC32 verified
* Confirmed in the same runs that the dump path stays inactive when the variables are unset
