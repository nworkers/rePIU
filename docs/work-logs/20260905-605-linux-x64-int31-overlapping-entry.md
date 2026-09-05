# 작업 로그: Linux x64 사설 INT 31h 중첩 엔트리 판별

> Task 606 정정: 바이트/레지스터 관찰은 유효하지만 의도적 진입 및 사설 ABI라는
> 결론은 철회합니다. 호출자 FPU 루틴의 word-stack 처리 누락이 반환주소를 손상시켰으며,
> 수정 후 `1E7Fh` 호출이 사라졌습니다. 아래는 당시의 분석 기록입니다.
>
> Task 606 correction: the byte/register observations stand, but the intentional-entry
> and private-ABI conclusions are withdrawn. Missing word-stack lowering corrupted
> the caller's return address; fixing it removes the `1E7Fh` call. Below is the historical record.

## 한국어

### 수행 내용

Task 604 이후 fault 지점 주변을 원본 LE object 2와 실행 trace로 대조했습니다. object 2는 relocated guest base `0x01010000`을 사용하며, `0x010F0107`은 object 2 offset `0xE0107`에 해당합니다.

확인한 원본 바이트는 다음과 같습니다.

```text
guest 0x010F0104: 66 8B 4D 1E
guest 0x010F0107: 1E 66 8B 55 1C CD 31
guest 0x010F010C: CD 31
```

`0x010F0104`에서 순차 실행하면 `66 8B 4D 1E`의 `1E`는 displacement입니다. 별도 AOT 엔트리인 `0x010F0107`에서 시작하면 같은 바이트가 유효한 `PUSH DS` opcode가 되고, 이어서 `MOV DX,[EBP+1C]`와 `INT 31h`가 실행됩니다. 이는 x86에서 가능한 의도적인 중첩 엔트리이며, AOT가 instruction 중간으로 잘못 점프했다는 증거가 아닙니다.

실행 trace도 동일한 결론을 보였습니다.

```text
[repiu-aot-fault] cache=0x2004FDCE ... guest=0x010F0107
[repiu-exec-trace] #0 eip=0x010F0107 ... eax=0x00001E7F
[repiu-exec-trace] #1 eip=0x010F010C ... eax=0x00001E7F
[repiu-dpmi-1e7f] ... probe-success=0
```

따라서 주변의 `MOV EAX,7`을 실행시키기 위해 AOT reverse map이나 guest entry를 수정할 근거가 없습니다. 실제 제품 경계는 호출자가 준비한 `AX=1E7Fh`의 사설 서비스 계약입니다. 기본 구현은 `AX=8001h`와 CF 설정을 반환하고, 그 결과의 후속 error path가 `0x010F4AD2`에서 `EBX=0` null write에 도달합니다. 진단용 probe-success는 이 경로를 바꾸는 ABI 구현이 아닙니다.

이번 작업에서는 소스 코드를 변경하지 않았습니다. Task 604에서 확인한 빌드 및 core probe 결과는 다음과 같습니다.

```text
far_return_all=true
core_probe_total=23
core_probe_failures=0
core_probe_all=true
```

### 결론

* `0x010F0107`은 유효한 중첩 엔트리입니다.
* `0x010F010C`의 `INT 31h`는 caller-prepared `AX=1E7Fh`를 사용합니다.
* `0x010F4AD2`는 오류 경로의 실제 null write frontier입니다.
* 다음 구현 작업은 AOT 정렬 보정이 아니라 사설 `1E7Fh` ABI의 원본 근거 확보여야 합니다.

## English

### Work performed

The fault area after Task 604 was correlated with reconstructed LE object 2 and the execution trace. Object 2 uses relocated guest base `0x01010000`, so guest `0x010F0107` corresponds to object 2 offset `0xE0107`.

The original bytes are:

```text
guest 0x010F0104: 66 8B 4D 1E
guest 0x010F0107: 1E 66 8B 55 1C CD 31
guest 0x010F010C: CD 31
```

Sequential execution from `0x010F0104` treats the `1E` in `66 8B 4D 1E` as a displacement. Starting at the separate AOT entry `0x010F0107` makes the same byte a valid `PUSH DS` opcode, followed by `MOV DX,[EBP+1C]` and `INT 31h`. This is an intentional overlapping x86 entry pattern, not evidence that AOT jumped into the middle of an instruction incorrectly.

The execution trace agrees:

```text
[repiu-aot-fault] cache=0x2004FDCE ... guest=0x010F0107
[repiu-exec-trace] #0 eip=0x010F0107 ... eax=0x00001E7F
[repiu-exec-trace] #1 eip=0x010F010C ... eax=0x00001E7F
[repiu-dpmi-1e7f] ... probe-success=0
```

There is therefore no basis to alter the AOT reverse map or guest entry to force execution of the nearby `MOV EAX,7`. The actual product boundary is the private service contract for caller-prepared `AX=1E7Fh`. The default implementation returns `AX=8001h` with CF set, and the resulting error path reaches the `EBX=0` null write at `0x010F4AD2`. The diagnostic probe-success switch is not an ABI implementation.

No source code was changed in this task. The build and core-probe results confirmed in Task 604 are:

```text
far_return_all=true
core_probe_total=23
core_probe_failures=0
core_probe_all=true
```

### Conclusion

* `0x010F0107` is a valid overlapping entry.
* The `INT 31h` at `0x010F010C` uses caller-prepared `AX=1E7Fh`.
* `0x010F4AD2` is the actual null-write frontier of the error path.
* The next implementation task should obtain original evidence for the private `1E7Fh` ABI, not add an AOT alignment workaround.
