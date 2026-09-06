# 20260906-617 Linux x64 zero RET site 작업 로그

## 한국어

### 결과

direct `RET` emitter가 producer site를 thunk frame metadata로 전달하도록
확장했습니다. `pumpit2a`의 zero-source event는 다음 guest `RET` site로
식별됐습니다.

```text
producer=ret producer_site=0x010F101D
guest_esp=0x0158CC4C stack_base=0x0158CC44 valid=0xF
m8=0x0128E488 m4=0x00000000 p0=0x010F1026 p4=0x011A7AE0 matches=0x2
```

따라서 원래 `RET` 실행 시점의 stack pointer는 `0x0158CC48`이고, 그 위치의
zero word가 반환 source로 소비됐습니다. top tracked call의 기대 fallthrough는
`0x010F4AD1`이지만 관찰된 source는 0입니다.

`REPIU_AOT_GUEST_MAP_TRACE=0xF101D`로 같은 site의 초기 AOT map도 확인했습니다.
guest address는 exact match 1개, map index `13859`, cache `0x2001487D`,
`guest_len=1`, `emitted_len=26`입니다. 즉 provenance tag가 임의 host address가
아니라 실제 guest return record에 부착됐습니다.

### 검증

* Linux x64 `repiu_core_probe`: `core_probe_total=24`,
  `core_probe_failures=0`, `core_probe_all=true`.
* Linux x64 `repiu`와 return thunk 빌드 성공.
* `producer_site=0x010F101D` 확인.
* zero return slot `0x0158CC48`와 주변 stack window 확인.
* resolver/fault recovery 및 guest return target 동작은 변경하지 않았습니다.

### 다음 작업

`0x010F101D` 직전까지의 guest stack writer를 찾아 `0x0158CC48`에 zero가
기록된 이유를 확인해야 합니다. writer가 direct-call push인지, HLE/DOS 경로인지,
초기 guest stack 값인지 확인하기 전에는 zero를 보정하지 않습니다.

## English

### Result

The direct `RET` emitter now passes its producer site in thunk frame metadata.
The `pumpit2a` zero-source event identifies this guest `RET` site:

```text
producer=ret producer_site=0x010F101D
guest_esp=0x0158CC4C stack_base=0x0158CC44 valid=0xF
m8=0x0128E488 m4=0x00000000 p0=0x010F1026 p4=0x011A7AE0 matches=0x2
```

The stack pointer at the original `RET` was therefore `0x0158CC48`, and the
zero word at that address was consumed as the return source. The tracked top
call expects fallthrough `0x010F4AD1`, while the observed source is zero.

With `REPIU_AOT_GUEST_MAP_TRACE=0xF101D`, the same site appears as one exact
initial AOT map entry: map index `13859`, cache `0x2001487D`, `guest_len=1`,
and `emitted_len=26`. The provenance tag is attached to the real guest return
record rather than an arbitrary host address.

### Verification

* Linux x64 `repiu_core_probe`: `core_probe_total=24`,
  `core_probe_failures=0`, `core_probe_all=true`.
* Linux x64 `repiu` and return-thunk build succeeded.
* `producer_site=0x010F101D` was observed.
* Zero return slot `0x0158CC48` and its surrounding stack window were captured.
* Resolver/fault recovery and guest return-target behavior were not changed.

### Next task

Locate the guest stack writer immediately preceding `0x010F101D` that left zero
at `0x0158CC48`. Determine whether it is a direct-call push, an HLE/DOS path, or
initial guest-stack state before any zero is repaired.
