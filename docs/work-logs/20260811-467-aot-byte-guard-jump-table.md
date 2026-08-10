# 20260811-467 AOT byte-guard jump table 작업 로그 / AOT Byte-Guard Jump Table Work Log

설계: [20260811-467-aot-byte-guard-jump-table.md](../design/20260811-467-aot-byte-guard-jump-table.md)  
작업 지시: [20260811-467-aot-byte-guard-jump-table.md](../work-orders/20260811-467-aot-byte-guard-jump-table.md)

## 한국어

### 구현

- `JumpTableGuard`가 32-bit 직접 guard와 low-byte 정규화 필요 guard를 구분하도록 했습니다.
- AL/CL/DL/BL만 32-bit parent register로 연결하고 AH/CH/DH/BH는 거부합니다.
- 정확한 `and parent-r32,0xff` 한 명령을 통과한 경우에만 다음 주소로 guard를 전달합니다.
- 일반 walk와 방문 순서 독립 sweep 양쪽에서 같은 전달 규칙을 사용합니다. sweep는 정규화 전
  guard로 table branch를 재분류하지 못하도록 차단했습니다.
- 기존 `kJumpTable` target 검증, emitter와 Win32 fixup은 변경 없이 재사용했습니다.
- 독립 `--jump-table-guard` probe와 실제 image의 임의 주소에서 계획을 시작하는 `--entry`
  진단 옵션을 추가했습니다.

### 검증

- `repiu_aot_probe --jump-table-guard`: 성공 패턴, 잘못된 mask, 다른 register, high-byte
  compare, 정규화 생략과 기존 32-bit 형태를 모두 통과했습니다.
- 첫 probe 실행은 정규화 생략 case가 sweep에서 잘못 수락됨을 검출했습니다. sweep에도
  `requires_low_byte_normalization` 검사를 추가한 뒤 전 항목이 통과했습니다.
- `repiu_aot_probe PIU.EXE --entry 0x010E49D3`: instructions 1,295, jump tables 1,
  targets 10, HLE boundaries 0, cache jump-table sites 1, cache valid true였습니다. 이어 실행된
  전체 probe suite도 모두 통과했습니다.
- `cmake --build build\\win32_x86_debug --config Release --target repiu_aot_probe`: 성공.
- `cmake --build build\\win32_x86_debug --config Release --target repiu`: 성공.
- 전체 release batch의 최초 실행은 60초 tool timeout에 걸렸지만 core library 생성까지
  성공했으며, 필요한 두 Release target을 위 명령으로 별도 완료했습니다.

### 남은 확인

실제 게임 실행에서 로딩 시간 및 `0x030E49E4` boundary count 감소는 새 `repiu_log.txt`로
확인해야 합니다. 구현은 target 이름, 함수 주소와 zlib 식별자에 의존하지 않습니다.

## English

### Implementation

- Extended `JumpTableGuard` to distinguish direct 32-bit guards from guards requiring low-byte
  normalization.
- Mapped only AL/CL/DL/BL to their 32-bit parents and rejected AH/CH/DH/BH.
- Propagated a guard to the next address only across an exact `and parent-r32,0xff` instruction.
- Applied the same propagation rule in the normal walk and visit-order-independent sweep. The
  sweep is explicitly prevented from reclassifying a table branch with an unnormalized guard.
- Reused the existing `kJumpTable` target validation, emitter, and Win32 fixups unchanged.
- Added an isolated `--jump-table-guard` probe and an `--entry` diagnostic for planning from an
  arbitrary address in a real image.

### Verification

- `repiu_aot_probe --jump-table-guard`: passed the supported pattern, wrong mask, different
  register, high-byte compare, missing normalization, and existing 32-bit form cases.
- The first probe run detected that the sweep incorrectly accepted the missing-normalization case.
  Adding the normalization-state check to the sweep made every case pass.
- `repiu_aot_probe PIU.EXE --entry 0x010E49D3`: 1,295 instructions, one jump table, ten targets,
  zero HLE boundaries, one cache jump-table site, and a valid cache. The complete probe suite that
  followed also passed.
- `cmake --build build\\win32_x86_debug --config Release --target repiu_aot_probe`: passed.
- `cmake --build build\\win32_x86_debug --config Release --target repiu`: passed.
- The initial full Release batch hit the tool's 60-second timeout after successfully producing the
  core library; the two required Release targets were then completed separately as listed above.

### Remaining Verification

A new `repiu_log.txt` is required to measure the loading-time improvement and reduction of the
`0x030E49E4` boundary count in a live game run. The implementation does not depend on a target name,
function address, or zlib identifier.
