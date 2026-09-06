# Task 619 작업 로그: Linux x64 일반 AOT stack writer 추적

## 한국어

### 수행 내용

* `REPIU_LINUX_X64_STACK_TRACE=1`에서 일반 AOT `PUSH` lowering 뒤에도 기존
  Linux x64 stack-write ring 기록 sequence를 삽입했습니다.
* 실행 이력 321개를 보존하도록 ring 용량을 64개에서 512개로 늘렸습니다.
* zero return-frame 출력에서 기록을 `direct-call`과 `guest-push`로
  구분했습니다.

### 검증 결과

빌드:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

core probe:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

`pumpit2a`를 다음 옵션으로 실행한 결과, terminal zero `RET`는
`0x0158CC48`을 소비했고 해당 슬롯과 일치하는 stack-write 기록 11개가
출력됐습니다. 마지막 기록은 `site=0x010F0FE5`, `writer=guest-push`,
`value=0x00000000`이었습니다. AOT map에서 이 명령은 `PUSH ECX`이며,
직전 `0x010F0FE2`는 원본 바이트 `C2 04 00`인 `RET 4`였습니다.

```text
[repiu-x64-stack-write] slot=0x0158CC48 index=11 writer=guest-push site=0x010F0FE5 fallthrough=0x00000000 esp=0x0158CC48 value=0x00000000
```

### 결론

direct `CALL`의 반환 주소 기록 실패는 배제됐습니다. 마지막 zero는 정상적인
`PUSH ECX`가 기록한 값이며, x64 `RET` lowering이 `RET 4`의 즉시 스택 보정을
무시해 guest ESP를 4바이트 어긋나게 한 것이 다음 수정의 직접적인 결함으로
확인됐습니다. zero target 자동 복구는 수행하지 않았습니다.

## English

### Work performed

* Reused the Linux x64 stack-write ring after ordinary AOT `PUSH` lowering
  when `REPIU_LINUX_X64_STACK_TRACE=1`.
* Increased the ring capacity from 64 to 512 to retain the run's sequence 321
  history.
* Labeled matching records as `direct-call` or `guest-push` in the zero
  return-frame output.

### Verification

The Linux x64 build and core probe passed:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

In the `pumpit2a` reproduction, the terminal zero `RET` consumed
`0x0158CC48` and eleven stack-write records matched that slot. The last was
`site=0x010F0FE5`, `writer=guest-push`, and `value=0x00000000`. The AOT map
identifies this instruction as `PUSH ECX`; the preceding entry at
`0x010F0FE2` has original bytes `C2 04 00`, which is `RET 4`.

### Conclusion

Direct-call return-address writes are correct. The final zero is written by a
legitimate `PUSH ECX`, while the x64 near-return lowering incorrectly ignores
the immediate stack adjustment of `RET 4`, leaving guest ESP misaligned by four
bytes. No zero-target repair was performed.
