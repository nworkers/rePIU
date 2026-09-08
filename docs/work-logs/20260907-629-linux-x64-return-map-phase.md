# Task 629 작업 로그: Linux x64 return-time AOT guest map phase

설계: [20260907-629](../design/20260907-629-linux-x64-return-map-phase.md) ·
작업 지시: [20260907-629](../work-orders/20260907-629-linux-x64-return-map-phase.md) ·
분석: [linux-port-frontier 3.66](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

`TraceLinuxX64ReturnRegisters`가 선택한 return target에서 기존
`TraceAotGuestMap`을 `phase=return-trace`로 한 번 더 호출하게 했습니다. 새
환경 변수는 없습니다. `REPIU_AOT_GUEST_MAP_TRACE`가 없으면 기존과 같이 즉시
반환합니다. fault handler는 건드리지 않았습니다.

### 검증

빌드와 코어 프로브:

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

재현:

```text
REPIU_AOT_GUEST_MAP_TRACE=0x2A065,0x2A066,0x2A069,0x2A06C,0x2A072,0x2A07D,0x2A0A0,0xF1A80 \
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
./build/linux_x64/repiu pumpit2a
```

핵심 출력입니다.

```text
[repiu-aot-map-entry] target=0x0102A06C index=59420 cache=0x2005C28A guest_len=6 emitted_len=6 bytes=0F8D0CF8FFFF
[repiu-aot-map-fixup] source=0x0102A06C kind=conditional-branch target=0x01029F54 patch=0x0005C28C resolved=1
[repiu-aot-map-entry] target=0x010F1A80 index=59421 cache=0x2005C290 guest_len=1 emitted_len=7 bytes=458D7FFC41891F
```

진단 변수를 뺀 실행은 같은 fault를 재현했습니다.

### 확인한 사실

* `0x0102A06C`의 `jge`는 `0x2005C28A`에서 `0x2005C290`까지이고, fixup은 taken
  edge 하나뿐입니다. not-taken fallthrough인 `0x0102A072`로 가는 `E9`가
  없습니다.
* `0x2005C290`에는 같은 append가 emit한 `0x010F1A80`의 두 번째 사본,
  즉 `push ebx`가 있습니다.
* 따라서 `jge`가 성립하지 않으면 실행이 `0x0102A072`의 번역을 건너뛰고
  함수 prologue로 흘러갑니다. Task 628이 관측한 push 누락의 원인입니다.
* 같은 append의 `0x0102A0A0` 무조건 분기는 `E9`와 fixup을 정상적으로
  가집니다.
* `src/runtime/aot_code_cache.cpp`의 block fallthrough emission은 블록의 마지막
  명령이 `kCopy`일 때만 `E9`를 붙입니다. 조건 분기로 끝나는 블록은 받지
  못합니다.
* 이 지점 재현은 매번 성공하지 않습니다. 한 번은 `0x200008CB`의 다른 fault로
  끝났고 다음 시도에서 재현되었습니다.

### 판단

Task 628이 남긴 "push 누락의 원인"이 확정되었습니다. 이번 작업은 진단
추가이며 emitter를 고치지 않았으므로 fault는 그대로입니다. 수정은 다음
작업입니다.

### 다음 작업

조건 분기로 끝나는 블록에 fallthrough branch를 emit해야 합니다. 다음에
emit되는 명령이 fallthrough target일 때만 생략하는 방식과 i386 배치에 대한
영향을 함께 정해야 합니다.

## English

### Result

`TraceLinuxX64ReturnRegisters` now calls the existing `TraceAotGuestMap` a
second time, as `phase=return-trace`, at the selected return target. No new
environment variable was added: without `REPIU_AOT_GUEST_MAP_TRACE` the dump
returns immediately as before. The fault handler was not touched.

### Verification

Build and core probe:

```text
cmake --build build/linux_x64 --target repiu_core_probe repiu -j 4
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

Reproduction:

```text
REPIU_AOT_GUEST_MAP_TRACE=0x2A065,0x2A066,0x2A069,0x2A06C,0x2A072,0x2A07D,0x2A0A0,0xF1A80 \
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
./build/linux_x64/repiu pumpit2a
```

The decisive output:

```text
[repiu-aot-map-entry] target=0x0102A06C index=59420 cache=0x2005C28A guest_len=6 emitted_len=6 bytes=0F8D0CF8FFFF
[repiu-aot-map-fixup] source=0x0102A06C kind=conditional-branch target=0x01029F54 patch=0x0005C28C resolved=1
[repiu-aot-map-entry] target=0x010F1A80 index=59421 cache=0x2005C290 guest_len=1 emitted_len=7 bytes=458D7FFC41891F
```

A run without the diagnostic variables reproduced the same fault.

### Facts established

* The `jge` at `0x0102A06C` spans `0x2005C28A` to `0x2005C290` and carries only
  the taken-edge fixup. There is no `E9` for the not-taken fallthrough to
  `0x0102A072`.
* `0x2005C290` holds the second copy of `0x010F1A80` emitted by the same
  append — the `push ebx`.
* When the `jge` is not taken, execution therefore skips the translation of
  `0x0102A072` and runs into the function prologue. That is the cause of the
  missing push Task 628 observed.
* The unconditional branch at `0x0102A0A0` in the same append carries its `E9`
  and fixup correctly.
* The block-fallthrough emission in `src/runtime/aot_code_cache.cpp` appends the
  `E9` only when the block's last instruction is a `kCopy`; a block ending in a
  conditional branch receives none.
* Reaching this point does not reproduce on every attempt. One run ended in a
  different fault at `0x200008CB`, and the next reproduced it.

### Assessment

Task 628's open "cause of the missing push" is now settled. This task added
diagnostics and did not change the emitter, so the fault is unchanged. The fix
is the next task.

### Next task

Emit a fallthrough branch for a block ending in a conditional branch. Decide
whether to omit it only when the next emitted instruction is the fallthrough
target, and settle what this means for the i386 layout.
