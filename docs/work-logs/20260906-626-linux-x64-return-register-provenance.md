# Task 626 작업 로그: Linux x64 return frame register provenance

## 한국어

### 수행 내용

* `REPIU_LINUX_X64_RETURN_REG_TRACE=<guest-address>`를 추가했습니다.
* 선택된 return target의 x64 resolver frame에서 guest GPR, EIP, ESP,
  EFLAGS, producer tag, stack window를 기록했습니다.
* return thunk가 resolver 호출 전에 진입 시점 EFLAGS를 frame에 저장하도록
  했습니다.
* trace가 없을 때 기존 resolver와 guest 실행 정책은 변경하지 않았습니다.

### 검증

빌드:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

코어 프로브:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

재현:

```text
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
./build/linux_x64_repiu/repiu pumpit2a
```

관측 결과:

```text
[repiu-x64-return-reg] n=1 target=0x011A643A
  edi=0x0128CC2C esi=0x00000001 ebx=0x00000004 edx=0x00000000
  ecx=0x0128CC2C eax=0x00000000 ebp=0x0128DA68 eip=0x011A643A
  esp=0x0158CC48 eflags=0x00200246 status=0x010F1AF8
  stack_base=0x0158CC44 valid=0xF m4=0x011A643A m0=0x00000000
  p4=0x011A7B28 p8=0x00000000
```

* `status=0x010F1AF8`인 일반 `RET`가 stack slot
  `0x0158CC44`의 `0x011A643A`를 소비했습니다.
* resolver 진입 시 EAX는 0이었습니다.
* Task 624에서 확인한 dynamic bytes `0D E9 6B 01 37`는
  `OR EAX,0x37016BE9`이므로, return target 진입 후 EAX가 fault 주소로
  바뀌는 경로가 설명됩니다.
* 최종 fault는 `0x011A6440`의 `00 00`, access `0x37016BE9`에서
  유지되었습니다.

### 결론 및 다음 작업

return thunk의 register save/restore는 현재 EAX fault의 원인이 아닙니다.
잘못된 실행은 일반 RET가 guest stack에서 동적 data fragment 주소를
return target으로 소비한 뒤, 이를 code로 실행하는 경로입니다. 다음 작업은
`0x0158CC44`에 `0x011A643A`를 기록한 최초 stack writer를 추적해야 합니다.

## English

### Work performed

* Added `REPIU_LINUX_X64_RETURN_REG_TRACE=<guest-address>`.
* Recorded guest GPRs, EIP, ESP, EFLAGS, producer tag, and the stack window
  from the x64 resolver frame for one selected return target.
* Saved entry EFLAGS in the return thunk frame before calling the resolver.
* Preserved the existing resolver and guest execution policy when tracing is
  unset.

### Verification

Build:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

Core probe:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

Reproduction:

```text
REPIU_LINUX_X64_RETURN_REG_TRACE=0x011A643A \
./build/linux_x64_repiu/repiu pumpit2a
```

Observed state:

```text
[repiu-x64-return-reg] n=1 target=0x011A643A
  edi=0x0128CC2C esi=0x00000001 ebx=0x00000004 edx=0x00000000
  ecx=0x0128CC2C eax=0x00000000 ebp=0x0128DA68 eip=0x011A643A
  esp=0x0158CC48 eflags=0x00200246 status=0x010F1AF8
  stack_base=0x0158CC44 valid=0xF m4=0x011A643A m0=0x00000000
  p4=0x011A7B28 p8=0x00000000
```

* The ordinary `RET` identified by `status=0x010F1AF8` consumed
  `0x011A643A` from stack slot `0x0158CC44`.
* EAX was zero at resolver entry.
* Task 624 established that dynamic bytes `0D E9 6B 01 37` decode as
  `OR EAX,0x37016BE9`, explaining how EAX becomes the fault address after
  entering the return target.
* The final fault remained at `0x011A6440`, `00 00`, accessing
  `0x37016BE9`.

### Conclusion and next task

The return thunk's register save/restore is not the source of the EAX fault.
The invalid execution occurs when an ordinary RET consumes a dynamic data
fragment address from guest stack and executes it as code. The next task should
trace the first writer that placed `0x011A643A` into stack slot `0x0158CC44`.
