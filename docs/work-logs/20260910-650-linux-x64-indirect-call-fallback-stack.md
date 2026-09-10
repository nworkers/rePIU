# 20260910-650 작업 로그: Linux x64 간접 CALL fallback stack 의미 보존

설계: [20260910-650 설계](../design/20260910-650-linux-x64-indirect-call-fallback-stack.md)  
작업 지시: [20260910-650 작업 지시](../work-orders/20260910-650-linux-x64-indirect-call-fallback-stack.md)  
분석: [linux-port-frontier 3.87](../analysis/linux-port-frontier.md)

## 한국어

### 결과

공용 간접 전송 handler가 CALL의 반환 주소 write, ESP 감소, call-frame 기록을 대상 cache
해석 전에 수행하도록 순서를 바로잡았습니다. 선택형 host-dispatch miss tail도 CALL
fallback에서는 이미 적재된 반환 주소를 남기도록 `ESP += 4`, JMP에서는 기존 의미대로
`ESP += 8`을 생성합니다.

실제 `pumpit2a`에서 `0x010F4ACF CALL EAX`가 미매핑 `0x010F920C`로 향할 때
`0x0158CC70 = 0x010F4AD1`이 보존됨을 확인했습니다. 이전에는 대상의 첫
`PUSH EBX`가 이 slot을 덮었습니다.

### 검증

Linux x64 Debug `repiu`와 `repiu_core_probe` 빌드가 통과했습니다.

```text
indirect_fallback_call_return_preserved=true,jump_metadata_removed=true
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

실제 실행의 이전 최종 실패는 아직 남았지만 ESP가 4바이트 교정됐습니다.

```text
[repiu-x64-return] result=translation-failed source=0x00000000 producer=0x010F1E56 guest_esp=0x0158CC58
[repiu-guest-write-trace-tail] event=hle watch=0x0158CC54 execution=0x010F9212 destination=0x0158CC54 size=4 bytes=00000000
```

게임은 아직 정상 실행되지 않습니다. 다음 frontier는 `0x010F1E56` epilogue가 왜
`0x010F9212 PUSH FS`가 만든 slot까지 복원하지 못하는지 확인하는 것입니다.

## English

### Result

The shared indirect-transfer handler now commits a CALL's return-address write, ESP
decrement, and call-frame record before target-cache resolution. The optional host-dispatch
miss tail emits `ESP += 4` for CALL fallback to retain that return address, and preserves
the existing `ESP += 8` behavior for JMP.

A real `pumpit2a` run confirmed that `0x010F4ACF CALL EAX` preserves
`0x0158CC70 = 0x010F4AD1` when its unmapped target is `0x010F920C`. The target's
first `PUSH EBX` previously overwrote this slot.

### Verification

Linux x64 Debug `repiu` and `repiu_core_probe` built successfully. The shared probe
reported 27/27 passes and verified the per-kind fallback layout.

The previous terminal failure remains, but ESP is corrected by four bytes. The new zero
slot at `0x0158CC54` is again written by `0x010F9212 PUSH FS`. The game still does not
run normally; the next frontier is why the `0x010F1E56` epilogue does not restore beyond
that saved segment slot.
