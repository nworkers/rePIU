# DS 메모리 읽기 HLE 작업 로그

## 변경 내용

`piu_1st`의 `8B 06` 정지점을 `enable_segment_load_hle` 경로에서 처리하도록 추가했다. `DS` shadow selector가 존재하고 `ESI=0`인 경우 `mov eax, dword ptr ds:[esi]`를 0으로 응답한다.

검증 중 이어서 관측된 `80 3E 00`, `AC`, `A4`도 같은 DS low-memory zero-read 정책으로 처리했다. 이 처리는 `DS` shadow selector가 존재하고 `ESI < 0x10000`인 경우로 제한한다.

segment memory load 기록에는 width를 추가했다. loader 로그는 width가 4인 경우 `Hex32`, 그 외에는 기존처럼 `Hex8`로 값을 출력한다.

## 검증 결과

`scripts\test_all.ps1`을 실행해 빌드와 `dos4gw_hello` 실행을 확인했다. `piu_1st`는 기존 `[8B] 06` 지점을 통과했고, 마지막 처리된 segment memory load는 `A4`였다.

현재 다음 정지점은 `0x020FBAF8`의 `CD 21`이다. 예외 시점의 `EAX=0x025DED2B`이므로 다음 작업은 `INT 21h AH=0xED` 계열 서비스의 의미를 분석하는 것이다.

`scripts\test_openwatcom_samples.ps1 -CompareBaseline`도 실행했다. 전체 819개 샘플 기준 baseline 회귀는 0건이며, pass 수는 기존과 동일하게 419개였다.

## 테스트 갱신

`scripts\test_all.ps1`의 `piu_1st` 기대 관측점을 이전 `[8B] 06`에서 새 `INT imm8` 정지점으로 갱신했다.

# DS Memory Read HLE Work Log

## Changes

Added handling for the `piu_1st` `8B 06` stop through the `enable_segment_load_hle` path. When a DS shadow selector exists and `ESI=0`, `mov eax, dword ptr ds:[esi]` returns zero.

The subsequently observed `80 3E 00`, `AC`, and `A4` instructions are also handled through the same DS low-memory zero-read policy. This is limited to cases where a DS shadow selector exists and `ESI < 0x10000`.

Segment memory load records now include width. Loader logs print `Hex32` for width 4 and keep `Hex8` for other widths.

## Verification Result

Ran `scripts\test_all.ps1` to verify the build and `dos4gw_hello` execution. `piu_1st` passed the previous `[8B] 06` point, and the last handled segment memory load was `A4`.

The current next stop is `CD 21` at `0x020FBAF8`. Since exception-time `EAX=0x025DED2B`, the next task is to analyze the `INT 21h AH=0xED` service family.

Also ran `scripts\test_openwatcom_samples.ps1 -CompareBaseline`. Across all 819 samples, baseline regressions are 0 and the pass count remains 419.

## Test Update

Updated the `piu_1st` expected observation point in `scripts\test_all.ps1` from the previous `[8B] 06` stop to the new `INT imm8` stop.
