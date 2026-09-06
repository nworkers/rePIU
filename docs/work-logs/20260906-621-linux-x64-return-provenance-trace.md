# Task 621 작업 로그: Linux x64 return target provenance 추적

## 한국어

### 변경 내용

* `REPIU_LINUX_X64_RETURN_TRACE`에 resolver target과 함께 producer tag,
  post-`RET` guest ESP를 출력하도록 확장했습니다.
* resolved return의 consumed slot(`guest_esp - 4`)과 일치하는 AOT
  stack-write ring record를 출력하도록 추가했습니다.
* 실행 sequence 13676을 보존할 수 있도록 x64 diagnostic ring을 512개에서
  16384개로 확장했습니다.
* trace가 없을 때 guest control-flow와 target resolution은 변경하지
  않습니다.

### 검증 결과

Linux x64 빌드와 core probe를 통과했습니다.

```text
guest_ret_imm16=true adjustment=8
linux_x64_guest_register_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

`pumpit2a` 실행에서 다음 반환 provenance를 확인했습니다.

```text
[repiu-x64-return] result=resolved source=0x011A643A cache=0x20126E56 producer=0x010F1AF8 guest_esp=0x0158CC48 detail=
[repiu-x64-return-stack] source=0x011A643A producer=0x010F1AF8 consumed=0x0158CC44 sequence=13676 matches=15
```

producer `0x010F1AF8`의 AOT map entry는 plain `RET`이며 emitted code는
guest ESP를 4바이트 증가시킵니다. 같은 consumed slot에 대해 site
`0x010F12BF`가 과거에 `0x011A643A`를 기록한 AOT ring record도 확인했습니다.

```text
[repiu-x64-return-stack] index=485 writer=guest-push site=0x010F12BF fallthrough=0x00000000 esp=0x0158CC44 value=0x011A643A
```

다만 이는 historical writer입니다. 같은 slot의 더 늦은 AOT record가
존재하며, HLE 또는 아직 ring에 기록하지 않는 경로가 마지막 값을 썼을
가능성도 있으므로 `0x010F12BF`를 최종 writer로 확정하지 않았습니다.

반환 target `0x011A643A`는 initial AOT map entry가 아니라 dynamic AOT로
resolve되었고, 실행은 `0x011A6440`의 data-like bytes에서 계속 fault했습니다.
따라서 다음 작업은 AOT ring 밖의 guest-memory write와 해당 dynamic target의
translation bytes를 추적하는 것으로 한정됩니다. return target 자동 수정은
수행하지 않았습니다.

### 결론

return producer, post-`RET` ESP, consumed slot 및 과거 AOT writer는
확인했습니다. 최종 last-writer 귀속과 `0x011A6440` fault 원인은 아직
미확정이며 다음 Linux x64 분석 frontier로 남겼습니다.

## English

### Changes

* Extended `REPIU_LINUX_X64_RETURN_TRACE` with the producer tag and post-return
  guest ESP alongside the resolver target.
* Added output for AOT stack-write ring records matching the resolved return's
  consumed slot (`guest_esp - 4`).
* Expanded the x64 diagnostic ring from 512 to 16384 records so sequence 13676
  remains available.
* Kept guest control flow and target resolution unchanged when tracing is off.

### Verification

The Linux x64 build and core probe passed:

```text
guest_ret_imm16=true adjustment=8
linux_x64_guest_register_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

The `pumpit2a` run produced this return provenance:

```text
[repiu-x64-return] result=resolved source=0x011A643A cache=0x20126E56 producer=0x010F1AF8 guest_esp=0x0158CC48 detail=
[repiu-x64-return-stack] source=0x011A643A producer=0x010F1AF8 consumed=0x0158CC44 sequence=13676 matches=15
```

The producer map entry at `0x010F1AF8` is a plain `RET`, and its emitted code
advances guest ESP by four bytes. A retained AOT ring record also shows that
site `0x010F12BF` previously wrote `0x011A643A` to the consumed slot:

```text
[repiu-x64-return-stack] index=485 writer=guest-push site=0x010F12BF fallthrough=0x00000000 esp=0x0158CC44 value=0x011A643A
```

This is historical provenance, not a final-writer proof. Later AOT records for
the same slot exist, and an HLE or otherwise uninstrumented path may have
written the final value, so `0x010F12BF` remains a candidate rather than a
confirmed last writer.

The return target `0x011A643A` has no initial AOT map entry; it resolves through
dynamic AOT, and execution still faults at `0x011A6440` on data-like bytes. The
next task is limited to tracing guest-memory writes outside the AOT ring and
dumping the dynamic translation bytes for that target. No return-target repair
was performed.

### Conclusion

The return producer, post-return ESP, consumed slot, and historical AOT writer
are confirmed. Final last-writer attribution and the cause of the
`0x011A6440` fault remain unresolved and are the next Linux x64 frontier.
