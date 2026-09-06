# 20260906-616 Linux x64 반환 source provenance 작업 로그

## 한국어

### 결과

공용 `RepiuLinuxX64ReturnThunk`에 producer tag를 전달하고, resolver가
`guest_source=0`을 받는 순간의 frame과 guest stack을 기록했습니다. direct
`RET`는 `R10D=0`, indirect call은 `R10D=1`을 설정하며 thunk는 이를 frame
`status`에 보존합니다.

`pumpit2a` 실행에서 다음 결과를 얻었습니다.

```text
[repiu-x64-return-frame] n=1 source=0x00000000 guest_eip=0x00000000 guest_esp=0x0158CC4C eflags=0x00000000 continuation=0x00000000 metadata_esp=0x00000000 status=0x00000000 stack_base=0x0158CC44 valid=0xF m8=0x0128E488 m4=0x00000000 p0=0x010F1026 p4=0x011A7AE0 matches=0x2 producer=ret last_indirect=0x010F4ACF/0x0103B140 last_return=0x00000000/0x00000000 call_depth=12 top_call=0x010F4ACF/0x0103B140/0x010F4AD1/0x0158CC74/0
[repiu-x64-return] result=translation-failed source=0x00000000 cache=0x00000000 detail=dynamic AOT target is outside the guest arena
```

`producer=ret`와 `matches=0x2`가 함께 확인되므로, 이번 frontier는 indirect
call의 zero target이 아니라 direct `RET`가 `guest ESP-4`의 zero word를
소비한 경우입니다. thunk frame의 guest ESP는 pop 이후 값
`0x0158CC4C`이고, 반환 resolver에 전달된 source/EIP는 0입니다.

현재 top tracked call의 기대 반환 주소는 `0x010F4AD1`이므로 zero return
word는 정상적인 tracked call fallthrough와도 일치하지 않습니다. 따라서
반환 정책을 바꾸기 전에 zero word가 기록된 guest stack slot과 해당 `RET`
site를 찾아야 합니다.

### 검증

* Linux x64 Debug `repiu_core_probe`: `core_probe_total=24`,
  `core_probe_failures=0`, `core_probe_all=true`.
* Linux x64 `repiu` 및 x64 return thunk assembly 빌드 성공.
* producer tag가 `producer=ret`로 관찰됨.
* `guest ESP-4=0`, `guest ESP=0x010F1026`, valid mask `0xF` 확인.
* 실행은 기존 `0x401C...` x64 fail-closed `INT3`/`UD2` 경계에서 종료했으며,
  resolver 정책은 변경하지 않았습니다.

### 다음 작업

direct `RET` emitter가 자신의 guest site를 frame metadata에 남기도록 bounded
diagnostic provenance를 확장하고, zero return word가 생기는 `RET` site와
그 직전 stack writer를 특정합니다.

## English

### Result

The shared `RepiuLinuxX64ReturnThunk` now receives a producer tag, and the
resolver records the frame and guest stack when it receives `guest_source=0`.
An ordinary `RET` sets `R10D=0`; an indirect call sets `R10D=1`; the thunk
preserves the tag in frame `status`.

The `pumpit2a` run produced:

```text
[repiu-x64-return-frame] n=1 source=0x00000000 guest_eip=0x00000000 guest_esp=0x0158CC4C eflags=0x00000000 continuation=0x00000000 metadata_esp=0x00000000 status=0x00000000 stack_base=0x0158CC44 valid=0xF m8=0x0128E488 m4=0x00000000 p0=0x010F1026 p4=0x011A7AE0 matches=0x2 producer=ret last_indirect=0x010F4ACF/0x0103B140 last_return=0x00000000/0x00000000 call_depth=12 top_call=0x010F4ACF/0x0103B140/0x010F4AD1/0x0158CC74/0
[repiu-x64-return] result=translation-failed source=0x00000000 cache=0x00000000 detail=dynamic AOT target is outside the guest arena
```

`producer=ret` together with `matches=0x2` identifies this frontier as an
ordinary `RET` consuming a zero word at guest `ESP-4`, rather than an indirect
call with a zero target. The thunk frame's guest ESP is the post-pop value
`0x0158CC4C`, and the source/EIP presented to the resolver is zero.

The tracked top call expects fallthrough `0x010F4AD1`, so the zero return word
does not match the normal tracked-call fallthrough. The zero stack writer and
the guest `RET` site must be located before changing return policy.

### Verification

* Linux x64 Debug `repiu_core_probe`: `core_probe_total=24`,
  `core_probe_failures=0`, `core_probe_all=true`.
* Linux x64 `repiu` and x64 return-thunk assembly built successfully.
* The producer tag was observed as `producer=ret`.
* Guest `ESP-4=0`, guest `ESP=0x010F1026`, and valid mask `0xF` were captured.
* Execution still stops at the existing x64 fail-closed `INT3`/`UD2` boundary;
  resolver policy was not changed.

### Next task

Extend bounded diagnostic provenance so a direct `RET` emitter records its guest
site in frame metadata, then identify the `RET` site and preceding stack writer
that produced the zero return word.
