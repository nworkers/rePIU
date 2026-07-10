# 0x02A0 Port I/O 보류 통과 작업 로그

## 결과

`0x02A0..0x02AF` 범위의 4바이트 `OUT DX,EAX`를 `deferred-ignored`로 제한 통과하도록 변경했다. trace buffer는 앞쪽 관측 샘플 16개만 보존하고, buffer가 찼다는 이유만으로 실행을 중단하지 않는다.

`IN EAX,DX`는 아직 응답 모델이 없으므로 기록 가능한 중단점으로만 추가했다.

## 관측

`piu_1st`는 기존의 인위적인 Port I/O trace-limit를 지나 다음 중단점에 도달했다.

* 예외 코드: `0xC0000005`
* 예외 위치: relocated base + `0x000F4955`
* 예외 바이트: `CD 21`
* 레지스터: `EAX=0x00003509`
* 의미: `INT 21h AH=35h AL=09h`, interrupt 09h vector 조회
* 마지막 Port I/O: relocated base + `0x000F4386`, `OUT DX,EAX`, port `0x02A2`, value `0x000000D0`, result `deferred-ignored`

`0x02A0` 계열의 실제 장치 의미는 아직 확정하지 않았고 `docs/TODO.md`에 남겨둔 분석 항목을 유지한다.

## 검증

* 통과: `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* 참고: Win32 x86 빌드 중 서드파티 `spdlog` 헤더의 기존 `C4819` 경고가 출력되었다.

# Deferred 0x02A0 Port I/O Pass Work Log

## Result

4-byte `OUT DX,EAX` operations in the `0x02A0..0x02AF` range now pass as `deferred-ignored` with a separate safety limit. The trace buffer keeps only the first 16 observation samples and no longer stops execution just because the buffer is full.

`IN EAX,DX` was added only as a recordable stopping point because no response model exists yet.

## Observation

`piu_1st` moved past the previous artificial Port I/O trace limit and reached the next blocker.

* Exception code: `0xC0000005`
* Exception location: relocated base + `0x000F4955`
* Exception bytes: `CD 21`
* Register: `EAX=0x00003509`
* Meaning: `INT 21h AH=35h AL=09h`, get interrupt 09h vector
* Last Port I/O: relocated base + `0x000F4386`, `OUT DX,EAX`, port `0x02A2`, value `0x000000D0`, result `deferred-ignored`

The real device meaning of the `0x02A0` family is still not classified, and the deferred analysis item remains in `docs/TODO.md`.

## Verification

* Passed: `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* Note: The Win32 x86 build still emits the existing third-party `spdlog` header `C4819` warning.
