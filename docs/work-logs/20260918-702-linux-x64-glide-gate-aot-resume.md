# Task 702 작업 로그 — Linux x64 Glide gate 반환 AOT 재진입

## 결과

fault-level Glide gate handler가 guest continuation으로 EIP를 진행시키면 Linux
x64에서 기존 `TryResumeAotAfterHandledHle`을
`kHandledGuestBoundary` origin으로 호출하도록 연결했습니다. cache hit, dynamic
translation, span/quarantine 검사와 long-mode fail-closed 정책은 기존 공용 경로를
재사용합니다. i386과 AOT 미사용 경로, Glide handler의 ABI cleanup은 변경하지
않았습니다.

원본 caller는 init 반환 직후 nonzero pointer를 `PUSH`하지만, 수정 전에는 이
명령이 host RSP에 실행되어 query guest stack에서 사라졌습니다. 수정 후 focused
trace는 `0x01055B03` continuation이 cache `0x200521DE`로 재진입한 것을 확인했습니다.

## 검증

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 통과
* Linux x64 Debug `repiu`: 빌드 통과
* init gate: ESP `0x0158C884`
* query gate: ESP `0x0158C880`, argument `0x0128FB30`
* `_GRSSTQUERYHARDWARE@4` memory failure 제거
* `_GRSSTSELECT@4`, `_GRSSTWINOPEN@28` 및 후속 state gate 진입
* 실제 실행에서 총 50회 Glide gate 진입

빌드에는 기존 extern 초기화 warning과 Windows-mounted tree의 clock-skew warning이
있었지만 두 target 모두 링크됐고 core probe가 통과했습니다.

다음 frontier는 두 번째 `_GRDITHERMODE@4` 반환 뒤의 unresolved return입니다.
SIGTRAP RIP `0x402CBE6B`은 `RepiuLinuxX64LegacyResumeThunk` 시작이고, 실제 trap
byte는 직전 `RepiuLinuxX64ReturnThunk`의 unresolved `INT3` (`0x402CBE6A`)입니다.
다음 작업에서 guest return target과 resolver zero 결과를 추적해야 합니다.

## English

### Result

When a fault-level Glide handler advances EIP to a guest continuation, Linux
x64 now calls the existing `TryResumeAotAfterHandledHle` with
`kHandledGuestBoundary` origin. Existing cache-hit, dynamic-translation,
span/quarantine, and long-mode fail-closed policies remain shared. i386,
non-AOT paths, and Glide ABI cleanup are unchanged.

The original caller pushes a nonzero pointer immediately after init returns.
Before the fix, long mode applied that instruction to host RSP and it vanished
from the query guest stack. After the fix, a focused trace confirmed that
continuation `0x01055B03` re-entered cache `0x200521DE`.

### Verification

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 passed
* Linux x64 Debug `repiu`: build passed
* Init gate: ESP `0x0158C884`
* Query gate: ESP `0x0158C880`, argument `0x0128FB30`
* Removed the `_GRSSTQUERYHARDWARE@4` memory failure
* Entered `_GRSSTSELECT@4`, `_GRSSTWINOPEN@28`, and later state gates
* Reached 50 total Glide-gate entries in the real run

The build emitted the existing extern-initialization warning and clock-skew
warnings from the Windows-mounted tree, but both targets linked and the core
probe passed.

The next frontier is an unresolved return after the second
`_GRDITHERMODE@4`. SIGTRAP RIP `0x402CBE6B` begins
`RepiuLinuxX64LegacyResumeThunk`; the actual trap byte is the preceding
unresolved `INT3` in `RepiuLinuxX64ReturnThunk` at `0x402CBE6A`. A follow-up
task should trace the guest return target and the resolver's zero result.
