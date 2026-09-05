# 작업 지시서: Linux x64 사설 INT 31h 중첩 엔트리 판별

## 작업 목표

`0x010F010C`에서 관찰되는 `INT 31h`가 AOT가 잘못된 instruction boundary에서 시작한 결과인지 확인하고, Task 604 이후의 실제 제품 blocker를 확정합니다.

## 실행 계획

1. 현재 branch와 작업 트리 상태를 확인합니다.
2. `PIU.EXE` LE object 2를 재구성하여 `0x010F0107` 주변의 원본 바이트를 확인합니다.
3. AOT fault trace, guest watch, execution trace를 같은 주소 범위에 대해 수집합니다.
4. `PUSH DS` 중첩 엔트리와 `INT 31h AX=1E7Fh` 호출 전후 레지스터를 대조합니다.
5. 결론과 미확정 ABI를 analysis, design, work-log 문서에 기록합니다.

## 예상 결과와 중단 기준

* `0x010F0107`이 유효한 AOT 엔트리이고 원본 바이트가 중첩 디코드로 설명되면 AOT 정렬 수정은 하지 않습니다.
* `AX=1E7Fh`의 성공 출력·메모리 효과·반환 프레임이 확인되지 않으면 해당 ABI를 추정 구현하지 않습니다.
* 기본 오류 경로가 `0x010F4AD2`의 `EBX=0` 쓰기까지 도달하는 것은 서비스 실패의 후속 증거로 기록하되, write를 삼키지 않습니다.

## 검증 기준

다음 증거를 모두 기록합니다.

```text
cache=0x2004FDCE -> guest=0x010F0107
guest 0x010F0107: 1E 66 8B 55 1C CD 31
guest 0x010F010C: CD 31
EAX before INT 31h: 0x00001E7F
default result: AX=0x8001, CF=1
```

소스 코드 변경이 없으므로 Task 604의 빌드·core probe 결과를 재사용하고, 이번 작업에서는 문서와 실행 trace의 일치 여부를 검증합니다.

## English

# Work Order: Linux x64 Private INT 31h Overlapping Entry Classification

## Objective

Determine whether the `INT 31h` observed at `0x010F010C` starts at an incorrect AOT instruction boundary, and establish the actual product blocker after Task 604.

## Execution plan

1. Confirm the current branch and worktree state.
2. Reconstruct LE object 2 and inspect original bytes around `0x010F0107`.
3. Collect AOT fault, guest-watch, and execution traces for the same address range.
4. Correlate the overlapping `PUSH DS` entry with registers before and after `INT 31h AX=1E7Fh`.
5. Record the conclusion and unresolved ABI in the analysis, design, and work-log documents.

## Expected result and stop criteria

* If `0x010F0107` is a valid AOT entry explained by overlapping original bytes, do not change AOT alignment.
* If the success outputs, memory effects, and return frame of `AX=1E7Fh` remain unconfirmed, do not implement a guessed ABI.
* Record the default error path reaching the `EBX=0` write at `0x010F4AD2` as follow-on evidence, but do not swallow the write.

## Verification criteria

Record all of the following evidence:

```text
cache=0x2004FDCE -> guest=0x010F0107
guest 0x010F0107: 1E 66 8B 55 1C CD 31
guest 0x010F010C: CD 31
EAX before INT 31h: 0x00001E7F
default result: AX=0x8001, CF=1
```

Because this task changes no source code, reuse Task 604's build and core-probe results and verify the consistency of the documentation and execution trace.

