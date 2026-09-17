# Task 701 작업 로그 — Linux x64 LINEXE GETPROCADDR guest CS

## 결과

LINEXE `GETPROCADDR` 성공 결과의 selector를 물리
`GuestCpuContext::SegCs`에서 가져오지 않고, wrapper continuation을 포함하는
유일한 실행 가능 guest descriptor에서 찾도록 변경했습니다. Linux x64 host
CS `0x33`이 guest ABI로 유출되던 경로가 제거되었으며 lookup 실패, 모호한 범위,
non-executable descriptor는 결과와 성공 반환 상태를 확정하기 전에 거부합니다.

합성 probe는 물리 CS `0x33`과 guest CS `0x24`를 분리하여 결과가
`{gate address, 0x24}`인지 검증합니다. guest code descriptor가 없을 때 결과
버퍼와 EIP/ESP/EAX가 바뀌지 않는 fail-closed 사례도 포함했습니다.

## 검증

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 통과
* 새 결과: `linexe_getproc_guest_cs=1,missing_selector_refused=1`
* Linux x64 Debug `repiu`: 빌드 통과
* 실제 `pumpit2a`: 첫 결과 far pointer `0x095D0300/0x00000024` 확인
* `_GRGLIDEINIT@0` ordinal 32와 `_GRSSTQUERYHARDWARE@4` ordinal 37 gate 진입 확인
* 기존 `Fatal error: unable to find entry point in DLL.` 미발생

실제 실행의 다음 frontier는 `_GRSSTQUERYHARDWARE@4`가 인자 포인터를 `0`으로
해석한 `query-hardware-unwritable-memory`입니다. 계속 처리한 뒤 host
`0x402CBDA3`에서 SIGTRAP이 처리되지 않았습니다. 이는 이번 selector 수정과
분리하여 guest call frame 및 Glide gate stack decode를 조사해야 합니다.

첫 30초 실행은 LINEXE 진입 전에 외부 timeout으로 종료되어 판정에 사용하지
않았습니다. 두 번째 실행은 실제 두 Glide gate 진입과 새 frontier를 확보했지만
정상 종료가 아니라 SIGTRAP/core dump로 끝났습니다.

## English

### Result

The successful LINEXE `GETPROCADDR` result now derives its selector from the
unique executable guest descriptor containing the wrapper continuation instead
of physical `GuestCpuContext::SegCs`. This prevents Linux x64 host CS `0x33`
from crossing the guest ABI. Missing or ambiguous lookups and non-executable
descriptors are rejected before committing the output and successful return
state.

The synthetic probe separates physical CS `0x33` from guest CS `0x24` and
verifies `{gate address, 0x24}`. It also covers a missing guest-code descriptor,
confirming that the result buffer and EIP/ESP/EAX remain unchanged.

### Verification

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 passed
* New result: `linexe_getproc_guest_cs=1,missing_selector_refused=1`
* Linux x64 Debug `repiu`: build passed
* Real `pumpit2a`: first result far pointer `0x095D0300/0x00000024` confirmed
* Entered ordinal 32 `_GRGLIDEINIT@0` and ordinal 37
  `_GRSSTQUERYHARDWARE@4`
* The former `Fatal error: unable to find entry point in DLL.` did not occur

The next real frontier is `_GRSSTQUERYHARDWARE@4` decoding a zero argument
pointer and reporting `query-hardware-unwritable-memory`. Continuing afterward
ended in an unhandled SIGTRAP at host `0x402CBDA3`. Guest call-frame construction
and Glide-gate stack decoding should be investigated separately from this
selector fix.

The first 30-second run ended at its external timeout before LINEXE entry and
was not used as acceptance evidence. The second run captured both Glide gate
entries and the new frontier, but ended in SIGTRAP/core dump rather than normal
termination.
