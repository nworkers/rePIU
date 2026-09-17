# Task 700 작업 로그 — Linux x64 CS override 간접 점프 경계

## 결과

`0x010F777C`의 raw guest bytes를 `2E FF 24 9D 44 77 0F 01`로 확인하고, cache
breakpoint 뒤의 `67 0F B6 50 01`을 guest code로 오인했던 설계를 폐기했습니다.
재진입 판별과 기존 간접 전송 handler에 CS override를 연결하여 원본 jump-table
명령을 그대로 처리했습니다.

planner HLE provenance와 전송 판별이 겹칠 때 전송 handler가 우선하며, ModRM/SIB가
계산한 offset은 source code selector와 기존 `ResolveSegmentLinearRange` 정책으로
해석됩니다. 읽은 target은 기존 AOT resolver로 전달되므로 원본 게임 로직이나
target 정책을 새로 구현하지 않았습니다.

## 검증

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 통과
* 새 synthetic 결과: `cs_indirect_jump=1`
* Linux x64 Debug `repiu`: 빌드 통과
* 실제 `pumpit2a`: `0x010F777C`에서 `transfer=1`, SIGTRAP 없음
* Task 699 probe: generation 9에 설치되고 `configured/hit=true`
* snapshot: EIP `0x010F928B`, ESP `0x0158CC54`, EFLAGS `0x00200246`,
  EAX `0`, EBX `0x0158CCC0`, ECX `0`, EDX `0x0158CCC0`

실제 실행은 probe 이후 원본의 `Fatal error: unable to find entry point in DLL.`을
출력하고 DOS `4C01`로 종료했으며 host trampoline이 정상 회수했습니다. 이는 이번
경계 처리의 실패가 아니라 다음 DLL entry-point 기능 frontier입니다.

## English

### Result

Confirmed the raw guest bytes at `0x010F777C` as
`2E FF 24 9D 44 77 0F 01` and discarded the design that had mistaken
`67 0F B6 50 01` after the cache breakpoint for guest code. Reentry
classification and the existing indirect-transfer handler now cooperate to
execute the original CS-override jump-table instruction.

Transfer handling takes precedence when planner-HLE provenance overlaps the
decoded transfer. The ModRM/SIB offset is resolved through the source code
selector and the existing `ResolveSegmentLinearRange` policy, and the loaded
target is passed to the existing AOT resolver. No game logic or new target
policy was introduced.

### Verification

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 passed
* New synthetic result: `cs_indirect_jump=1`
* Linux x64 Debug `repiu`: build passed
* Real `pumpit2a`: `transfer=1` at `0x010F777C`, with no SIGTRAP
* Task 699 probe: installed in generation 9 and reported `configured/hit=true`
* Snapshot: EIP `0x010F928B`, ESP `0x0158CC54`, EFLAGS `0x00200246`,
  EAX `0`, EBX `0x0158CCC0`, ECX `0`, EDX `0x0158CCC0`

After the probe, the real run printed the original
`Fatal error: unable to find entry point in DLL.`, terminated through DOS
`4C01`, and returned through the host trampoline. This is the next DLL
entry-point functional frontier, not a failure of this boundary handling.
