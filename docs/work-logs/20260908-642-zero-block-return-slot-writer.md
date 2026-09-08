# Task 642 작업 로그: zero-block 반환 슬롯 writer 확인

설계: [20260908-642](../design/20260908-642-zero-block-return-slot-writer.md)  
작업 지시: [20260908-642](../work-orders/20260908-642-zero-block-return-slot-writer.md)  
분석: [linux-port-frontier 3.79](../analysis/linux-port-frontier.md)

## 한국어

기존 stack-write ring과 guest-write page 감시를 함께 사용했습니다. Task 641의 최초
보조 실행은 `REPIU_LINUX_X64_STACK_TRACE=1`을 빠뜨렸으므로 sequence 0이 writer
부재를 뜻하지 않았습니다. 옵션을 포함한 재실행에서는 sequence 62 중 소비 슬롯과
일치하는 writer가 정확히 하나 확인됐습니다.

```text
site=0x010F4A93 writer=guest-push esp=0x0158CC84 value=0x011A8E10
```

guest-write fault의 `EDI`도 `0x011A8E10`이었고, source map emitted bytes
`45 8D 7F FC 41 89 3F`는 R15D stack pointer를 4 감소시킨 뒤 EDI를 저장하는
`PUSH EDI` lowering입니다. 이후 `0x010F1E56 RET`가 이 saved-register 슬롯을
반환 주소로 소비합니다.

코드 변경은 없으므로 추가 빌드는 수행하지 않았습니다. 사용한 실행 파일은 Task 641에서
빌드되고 core probe `24/24`를 통과한 동일 바이너리입니다. 실제 추적은 exact writer와
source map을 모두 확인했습니다.

게임은 아직 정상 실행되지 않습니다. 다음 경계는 `0x010F4A93` 이후
`0x010F1E56`까지 guest ESP가 올바른 반환 슬롯으로 복원되지 않는 최초 지점입니다.

## English

Used the existing stack-write ring and guest-write page watcher together. The
first Task 641 companion run omitted `REPIU_LINUX_X64_STACK_TRACE=1`, so its
zero sequence did not mean the writer was absent. The corrected run recorded
62 writes and exactly one match for the consumed slot:

```text
site=0x010F4A93 writer=guest-push esp=0x0158CC84 value=0x011A8E10
```

The guest-write fault also had `EDI=0x011A8E10`. Source-map emitted bytes
`45 8D 7F FC 41 89 3F` lower `PUSH EDI` by subtracting four from the R15D guest
stack pointer and storing EDI. The later RET at `0x010F1E56` consumes this
saved-register slot as its return address.

No code changed, so no additional build was needed. The executable was the
same Task 641 binary that passed all 24 core-probe groups. The real traces
confirmed both the exact writer and its source map.

The game still does not run normally. The next boundary is the first point
between `0x010F4A93` and `0x010F1E56` where guest ESP fails to return to the
correct return-address slot.
