# Task 622 작업 로그: Linux x64 dynamic target dump

## 한국어

### 변경 내용

* `REPIU_AOT_DYNAMIC_TRACE=<guest-address>` opt-in parser를 추가했습니다.
* 지정한 dynamic append의 raw guest bytes, plan entry, emitted image entry를
  출력하도록 했습니다.
* trace가 없을 때 dynamic AOT acceptance와 emitted image는 변경하지
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

`REPIU_AOT_DYNAMIC_TRACE=0x011A643A` 실행에서 다음을 확인했습니다.

```text
[repiu-aot-dynamic] stage=raw guest=0x011A643A bytes=0DE96B0137060000F0E36B010900000005000000001000000000000000000000 length=138611654
[repiu-aot-dynamic] stage=plan-entry guest=0x011A643A bytes=0DE96B0137 length=5
[repiu-aot-dynamic] stage=plan-entry-meta guest=0x011A643A kind=0 length=5 mnemonic=489
[repiu-aot-dynamic] stage=image-entry guest=0x011A643A bytes=0DE96B0137 length=5
[repiu-aot-dynamic] stage=image-entry-meta guest=0x011A643A cache=0x20126E56 append_offset=0x00126E56 guest_len=5 emitted_len=5 active=1
```

raw bytes의 처음 5바이트는 `OR EAX,0x37016BE9`이고, 다음 주소
`0x011A643F`는 `PUSH ES`(`06`), `0x011A6440`은 `00 00`입니다. dynamic
plan은 첫 5바이트만 copy entry로 만들고 segment instruction은 기존 HLE
경계로 남겼습니다.

guest watch에서 cache fault 후 `0x011A643F` single-step이 확인되었고,
해당 시점 ESP는 `0x0158CC48`이었습니다. `PUSH ES` HLE 처리 후 ESP는
`0x0158CC44`로 감소했습니다. 다음 `0x011A6440` fault는
`EAX=0x37016BE9` 주소에 접근했습니다.

```text
[repiu-watch] event=fault guest=0x011A643F n=1 at=0x20126E5B esi=0x00000001 esp=0x0158CC48 ebx=0x00000004 eflags=0x00200202
[repiu-watch] event=step guest=0x011A643F n=1 at=0x011A643F le_bytes=0x09016BE3F0000006 esi=0x00000001 esp=0x0158CC48 ebx=0x00000004 eflags=0x00200302
[repiu-fault] unhandled signal=0xb rip=0x11a6440 eip=0x11a6440 access=0x37016be9 bytes=00 00 f0 e3 6b 01 09 00 00 00 05 00 00 00 00 10 ...
```

dynamic dump를 끈 동일 stack/return trace 실행도
`cache=0x20126E56`와 `0x011A6440` fault를 재현했습니다. 따라서 이번
진단은 frontier를 변경하지 않았습니다.

### 결론

dynamic entry 자체의 x64 emitted bytes 불일치는 찾지 못했습니다. 현재
문제는 정상적인 `PUSH ES` HLE 이후 guest `00 00` 명령이
`EAX=0x37016BE9`을 메모리 주소로 사용하는 경로입니다. return target 자동
보정이나 guest fault 무시는 수행하지 않았습니다.

## English

### Changes

* Added the opt-in `REPIU_AOT_DYNAMIC_TRACE=<guest-address>` parser.
* Added raw guest-byte, plan-entry, and emitted-image-entry output for the
  selected dynamic append.
* Kept dynamic-AOT acceptance and the emitted image unchanged when tracing is
  disabled.

### Verification

The Linux x64 build and core probe passed:

```text
guest_ret_imm16=true adjustment=8
linux_x64_guest_register_all=true
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

With `REPIU_AOT_DYNAMIC_TRACE=0x011A643A`, the run reported:

```text
[repiu-aot-dynamic] stage=raw guest=0x011A643A bytes=0DE96B0137060000F0E36B010900000005000000001000000000000000000000 length=138611654
[repiu-aot-dynamic] stage=plan-entry guest=0x011A643A bytes=0DE96B0137 length=5
[repiu-aot-dynamic] stage=plan-entry-meta guest=0x011A643A kind=0 length=5 mnemonic=489
[repiu-aot-dynamic] stage=image-entry guest=0x011A643A bytes=0DE96B0137 length=5
[repiu-aot-dynamic] stage=image-entry-meta guest=0x011A643A cache=0x20126E56 append_offset=0x00126E56 guest_len=5 emitted_len=5 active=1
```

The first five raw bytes are `OR EAX,0x37016BE9`; the next address,
`0x011A643F`, is `PUSH ES` (`06`), and `0x011A6440` contains `00 00`. The
dynamic plan emits only the first five bytes as a copy entry and leaves the
segment instruction at the existing HLE boundary.

The guest watch confirms the cache fault followed by a single-step at
`0x011A643F`, with ESP `0x0158CC48`. After `PUSH ES` HLE, ESP decreases to
`0x0158CC44`, and the next fault at `0x011A6440` attempts access through
`EAX=0x37016BE9`.

```text
[repiu-watch] event=fault guest=0x011A643F n=1 at=0x20126E5B esi=0x00000001 esp=0x0158CC48 ebx=0x00000004 eflags=0x00200202
[repiu-watch] event=step guest=0x011A643F n=1 at=0x011A643F le_bytes=0x09016BE3F0000006 esi=0x00000001 esp=0x0158CC48 ebx=0x00000004 eflags=0x00200302
[repiu-fault] unhandled signal=0xb rip=0x11a6440 eip=0x11a6440 access=0x37016be9 bytes=00 00 f0 e3 6b 01 09 00 00 00 05 00 00 00 00 10 ...
```

The same stack/return trace run with the dynamic dump disabled also resolved
`cache=0x20126E56` and reached the `0x011A6440` fault. The diagnostic did not
move the frontier.

### Conclusion

No x64 emitted-byte mismatch was found at the dynamic entry. The current issue
is the guest path after a legitimate `PUSH ES` HLE, where the guest `00 00`
instruction uses `EAX=0x37016BE9` as a memory address. No return-target repair
or guest-fault suppression was performed.
