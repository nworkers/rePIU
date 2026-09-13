# 20260913-676 Linux x64 스택 기준 high-byte 목적지 변환 작업 지시서

## 한국어

### 목적

`MOV AH/CH/DH/BH, [ESP 기반 메모리]`의 공통 long-mode lowering을 추가하여
host RSP 손상을 막고, 관측된 `0x010F3F0D` 경계를 원본 의미로 통과시킨다.

### 작업 범위

- Task 676 설계 문서의 제한된 opcode 형태를 classifier에 추가한다.
- R14B와 DL을 이용하는 flags 보존 lowering을 구현한다.
- long-mode compatibility probe에 positive/negative 검사를 추가한다.
- core probe 및 WSL runtime을 실행한다.
- 분석 문서와 작업 로그에 확인 결과를 반영한다.

### 완료 기준

1. `8A 64 24 2C`가 stack-pointer divergence로 분류되고 lowering된다.
2. lowered bytes가 long-mode에서 의도한 memory load와 high-byte write로
   decode된다.
3. 기존 source-only high-byte lowering과 unrelated high-byte forms의 정책이
   유지된다.
4. `repiu_core_probe`가 실패 없이 종료한다.
5. runtime이 `0x010F3F0D`에서 즉시 종료하지 않고 다음 frontier를 보고한다.

### 검증 명령

```text
wsl.exe -d Ubuntu-24.04 --cd /mnt/e/MYWORK/Projects/rePIU -- cmake --build build/linux_x64_debug --target repiu_core_probe repiu --parallel 2
wsl.exe -d Ubuntu-24.04 --cd /mnt/e/MYWORK/Projects/rePIU -- ./build/linux_x64_debug/repiu_core_probe
```

## English

### Objective

Add a common long-mode lowering for `MOV AH/CH/DH/BH, [ESP-based memory]` so
host RSP is not corrupted and the observed `0x010F3F0D` frontier continues with
the original guest meaning.

### Scope

- Add the restricted opcode shape defined by the Task 676 design to the
  classifier.
- Implement a flags-preserving lowering using R14B and DL.
- Add positive and negative checks to the long-mode compatibility probe.
- Run the core probe and WSL runtime.
- Record the results in the analysis document and work log.

### Done criteria

1. `8A 64 24 2C` is classified as a stack-pointer divergence and lowered.
2. The lowered bytes decode in long mode as the intended memory load and
   high-byte write.
3. Existing source-only high-byte lowering and unrelated high-byte policies
   remain unchanged.
4. `repiu_core_probe` exits without failures.
5. The runtime does not terminate immediately at `0x010F3F0D` and reports a
   later frontier.

### Verification commands

```text
wsl.exe -d Ubuntu-24.04 --cd /mnt/e/MYWORK/Projects/rePIU -- cmake --build build/linux_x64_debug --target repiu_core_probe repiu --parallel 2
wsl.exe -d Ubuntu-24.04 --cd /mnt/e/MYWORK/Projects/rePIU -- ./build/linux_x64_debug/repiu_core_probe
```
