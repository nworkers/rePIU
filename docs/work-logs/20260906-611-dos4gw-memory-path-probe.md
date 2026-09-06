# Task 611 작업 로그 — DOS/4GW memory path probe

## 한국어

### 목적

기본 DOS `AX=3000h` 응답을 변경하지 않은 채, 원본 PharLap-style memory
초기화 분기가 allocator headroom을 제공하는지 실행으로 확인했습니다.

### 구현

* `REPIU_DOS4GW_MEMORY_PATH_PROBE=pharlap`일 때만 `AX=3000h`의 high word를
  `0x4458`로 설정했습니다.
* low word `0x0007`, 기본값, 알 수 없는 환경변수 값은 유지했습니다.
* `AH=4Ah` 전후 DOS trace를 추가해 selector, EBX, requested end,
  allocator end, 결과, CF를 기록했습니다.

### 실행 증거

```text
[repiu-dos-int-context] phase=return ... eax=0x44580007 ...
[repiu-dos-int] #2 int=21 ax=4A24
[repiu-dos-resize] eip=0x010F179E selector=0x0024 requested_ebx=0x000011A8 requested_end=0x01021A80 allocator_end=0x01021A80 success=1 error=0x0000 cf=0
```

resize HLE는 성공 반환했지만, guest는 같은 file-structure 오류 문구를
출력하고 `AX=4C01h`로 종료했습니다. 따라서 probe는 원본 분기와
`AH=4Ah` 도달 여부를 확정했지만, memory contract 해결책은 아닙니다.

### 검증

* `cmake --build build/linux_x64_debug --target repiu -j 2`: 성공
* `repiu_core_probe`: `core_probe_total=24`, `core_probe_failures=0`,
  `core_probe_all=true` 확인
* 기본 실행: `AX=0007h`, `AH=4Ah` 미도달, 기존 오류 종료
* probe 실행: `AX=44580007h`, `AH=4Ah` 성공 반환, 동일 오류 종료
* 두 실행 모두 원본 EIP 우회나 allocator metadata injection 없음

### 결론

PharLap signature를 기본 응답으로 승격하지 않습니다. 다음 작업은
resize 이후 원본 allocator call/return과 전역 상태를 추적하여, 성공한
resize가 어떤 block/global을 갱신해야 하는지 확인하는 것입니다.

## English

### Objective

Test whether the original PharLap-style memory initialization path provides
allocator headroom without changing the default DOS `AX=3000h` response.

### Implementation

* Set only the `AX=3000h` high word to `0x4458` when
  `REPIU_DOS4GW_MEMORY_PATH_PROBE=pharlap` is enabled.
* Preserve low word `0x0007`, the default behavior, and unknown environment
  values.
* Added DOS tracing around `AH=4Ah` for selector, EBX, requested end,
  allocator end, result, and CF.

### Runtime evidence

```text
[repiu-dos-int-context] phase=return ... eax=0x44580007 ...
[repiu-dos-int] #2 int=21 ax=4A24
[repiu-dos-resize] eip=0x010F179E selector=0x0024 requested_ebx=0x000011A8 requested_end=0x01021A80 allocator_end=0x01021A80 success=1 error=0x0000 cf=0
```

Resize returns success, but the guest prints the same file-structure error and
terminates with `AX=4C01h`. The probe confirms the original branch and
`AH=4Ah` reachability, but it is not the memory-contract solution.

### Verification

* `cmake --build build/linux_x64_debug --target repiu -j 2`: passed
* `repiu_core_probe`: confirmed `core_probe_total=24`,
  `core_probe_failures=0`, and `core_probe_all=true`
* Default run: `AX=0007h`, no `AH=4Ah`, existing error termination
* Probe run: `AX=44580007h`, successful `AH=4Ah`, same error termination
* Neither run bypasses a guest EIP or injects allocator metadata

### Conclusion

The PharLap signature is not promoted to the default response. The next task
will trace original allocator call/return and global state after successful
resize to determine which block/global should be updated by the memory
contract.
