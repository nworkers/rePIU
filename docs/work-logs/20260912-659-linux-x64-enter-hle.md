# 20260912-659 작업 로그: Linux x64 guest ENTER HLE

## 한국어

### 구현

`HandleEnterInstruction`을 추가하고 shared HLE dispatcher의 `C8` opcode에
연결했습니다. handler는 `imm16` allocation과 `imm8 & 0x1F` nesting level을
읽고, 기존 guest `EBP`와 nested display를 guest memory에 구성한 뒤 guest
`EBP`, `ESP`, `EIP`를 갱신합니다. EFLAGS와 host stack은 건드리지 않습니다.

### 검증 결과

* Linux x64 Debug `repiu` 및 `repiu_core_probe` 빌드 성공
* `core_probe_total=27`
* `core_probe_failures=0`
* `core_probe_all=true`
* live trace:
  `ENTER 4,0`, old `EBP=0x0158C848`, `ESP=0x0158C818` →
  new `EBP=0x0158C814`, `ESP=0x0158C810`
* 이전 `0x010F316C` / `MOV [EBP-4],EAX` host-stack fault 재발 없음

### 새 frontier

수정 후 실행은 guest `EIP=0x010EFE5F`의 `83 C4 04` 주변까지 진행했습니다.
그 뒤 `RepiuLinuxX64ReturnThunk`의 unresolved `INT3`에서 `SIGTRAP`으로
중단되었습니다. 이 문제는 `ENTER` 처리와 별개의 return-dispatch frontier로
남겼습니다.

## English

### Implementation

Added `HandleEnterInstruction` and connected it to the shared HLE dispatcher for
opcode `C8`. The handler reads the `imm16` allocation and `imm8 & 0x1F` nesting
level, builds the old-EBP/nested display in guest memory, then updates guest
`EBP`, `ESP`, and `EIP`. It does not modify EFLAGS or the host stack.

### Verification

* Linux x64 Debug `repiu` and `repiu_core_probe` builds succeeded.
* `core_probe_total=27`
* `core_probe_failures=0`
* `core_probe_all=true`
* Live trace: `ENTER 4,0`, old `EBP=0x0158C848`, `ESP=0x0158C818` →
  new `EBP=0x0158C814`, `ESP=0x0158C810`
* The previous `0x010F316C` / `MOV [EBP-4],EAX` host-stack fault did not recur.

### New frontier

After the fix, execution reaches the area around guest `EIP=0x010EFE5F`,
original bytes `83 C4 04`. It then stops on `SIGTRAP` at the unresolved `INT3`
in `RepiuLinuxX64ReturnThunk`. This is a separate return-dispatch frontier,
left for the next task.

### References

* [Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 1](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-1-manual.pdf)
