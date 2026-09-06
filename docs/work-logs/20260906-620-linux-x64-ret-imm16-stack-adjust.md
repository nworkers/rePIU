# Task 620 작업 로그: Linux x64 `RET imm16` stack 보정

## 한국어

### 변경 내용

* long-mode near-return emitter가 원본 `C3`와 `C2 iw`를 구분하도록
  수정했습니다.
* `C2 iw`의 unsigned `imm16`을 읽어 `4 + imm16`만큼 guest `R15D`를
  증가시키도록 했습니다.
* guest flags를 보존하기 위해 `LEA`를 사용했으며, 짧은 displacement로
  표현할 수 없는 경우 32비트 displacement 형식을 사용합니다.
* malformed 또는 미지원 return bytes는 기존 fail-closed 경계로 남겼습니다.

### 검증

Linux x64 빌드:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

core probe:

```text
guest_ret_imm16=true adjustment=8
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

실행 중 `RET 4` map entry는 다음과 같이 `LEA ... +8`을 포함했습니다.

```text
[repiu-aot-map-entry] target=0x010F0FE2 index=13908 guest=0x010F0FE2 cache=0x200149C8 guest_len=3 emitted_len=26 inactive=0 bytes=458B37458D7F0849BCCCAC1C40000000
```

`pumpit2a` 실행에서는 기존 `0x010F101D` zero-return fault가 재현되지
않았습니다. 대신 `0x011A643A`가 dynamic AOT target으로 resolve된 뒤,
다음 주소 `0x011A6440`에서 fault가 발생했습니다. `0x011A6440`은 initial
AOT map entry가 아니고 fault 시 instruction bytes도 `00 00 ...`인
data-like 영역이므로, 다음 조사 대상은 이 target을 만든 return 또는
indirect control-flow 경로입니다.

### 결론

`RET 4` stack adjustment 누락은 수정 및 실행으로 확인됐습니다. 기존 zero
return frontier는 해소됐으며, zero target 자동 복구는 수행하지 않았습니다.

## English

### Changes

* The long-mode near-return emitter now distinguishes original `C3` and
  `C2 iw` bytes.
* It reads unsigned `imm16` from `C2 iw` and advances guest `R15D` by
  `4 + imm16`.
* It uses `LEA` to preserve guest flags and selects the 32-bit displacement
  form when the short form cannot represent the adjustment.
* Malformed or unsupported return bytes remain at the existing fail-closed
  boundary.

### Verification

The Linux x64 build passed:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

The core probe reported:

```text
guest_ret_imm16=true adjustment=8
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

The runtime map entry for `RET 4` contains the expected `LEA ... +8`:

```text
[repiu-aot-map-entry] target=0x010F0FE2 index=13908 guest=0x010F0FE2 cache=0x200149C8 guest_len=3 emitted_len=26 inactive=0 bytes=458B37458D7F0849BCCCAC1C40000000
```

In the `pumpit2a` run, the old zero-return fault at `0x010F101D` did not
recur. Execution instead resolved `0x011A643A` as a dynamic AOT target and
then faulted at `0x011A6440`. That address is not an initial AOT map entry and
the fault instruction bytes are `00 00 ...`, indicating a data-like region.
The next target is the return or indirect control-flow path that produced
`0x011A643A`.

### Conclusion

The missing `RET 4` stack adjustment is corrected and runtime-verified. The
previous zero-return frontier is resolved. No zero-target repair was performed.
