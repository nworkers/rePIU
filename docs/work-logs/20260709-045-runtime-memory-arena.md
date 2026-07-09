# Runtime Memory Arena 작업 로그

## 결과

`RuntimeMemoryArenaPlan`을 공용 runtime 계층에 추가하고 Win32 loader가 relocated base 선택과 image placement에 arena reserve size를 사용하도록 연결했다. 초기 정책은 기존 image reserve size에 `0x00010000` expansion slack을 더한 뒤 4KB로 정렬한다.

`piu_1st`는 기존 `0x020F8405`의 `C7 04 02 FF FF FF FF` write blocker를 통과했다. 현재 다음 중단 지점은 `0x020F5637`의 `CD 21`이며, 직전 `B4 3B` 때문에 `INT 21h AH=0x3B` current-directory 변경 요청으로 분류된다.

## 검증

다음 검증을 완료했다.

* `scripts\test_all.ps1`: 통과. `dos4gw_hello`는 `Hello, world!`를 출력했고, `piu_1st`는 arena reserve `0x005E7000`으로 placement된 뒤 `0x020F5637`의 `INT imm8` 지점까지 진행했다.
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`: 통과. 전체 819개 중 overall pass 419개, overall pass rate 51.2%, regression 0개, new pass 0개다.

# Runtime Memory Arena Work Log

## Result

Added `RuntimeMemoryArenaPlan` to the shared runtime layer and connected the Win32 loader so relocated-base selection and image placement use the arena reserve size. The initial policy adds `0x00010000` expansion slack to the existing image reserve size and aligns the result to 4KB.

`piu_1st` now passes the previous `C7 04 02 FF FF FF FF` write blocker at `0x020F8405`. The current next stop is `CD 21` at `0x020F5637`; because it is preceded by `B4 3B`, it is classified as an `INT 21h AH=0x3B` current-directory change request.

## Verification

Completed the following verification:

* `scripts\test_all.ps1`: passed. `dos4gw_hello` printed `Hello, world!`, and `piu_1st` advanced to the `INT imm8` stop at `0x020F5637` after placement with arena reserve `0x005E7000`.
* `scripts\test_openwatcom_samples.ps1 -CompareBaseline`: passed. Overall pass is 419 of 819, overall pass rate is 51.2%, regression count is 0, and new pass count is 0.
