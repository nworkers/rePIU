# 작업 로그 20260905-602 — Linux x64 미해결 반환 주소 provenance 확정

설계: [20260905-602](../design/20260905-602-linux-x64-unresolved-return-provenance.md)  
작업 지시: [20260905-602](../work-orders/20260905-602-linux-x64-unresolved-return-provenance.md)

## 결과

Task 601의 far jump HLE 이후 frontier를 probe-success runtime에서 재현했습니다.
`0x01100042`의 guest `INT3`는 한 번 소비되었고, 이전
`0x010F016B` far-jump SIGILL은 재발하지 않았습니다.

`0x010F0232` watch 결과는 다음과 같습니다.

```text
[repiu-watch] event=step guest=0x010F0232 n=1 at=0x010F0232
  le_bytes=0x0000C35D5F5E5B07 ... esp=0x0158CC58
[repiu-x64-return] result=translation-failed source=0x000000FF
  cache=0x00000000 detail=dynamic AOT target is outside the guest arena
```

Little-endian bytes `07 5B 5E 5F 5D C3`는 `POP ES`, `POP EBX`, `POP ESI`,
`POP EDI`, `POP EBP`, `RET`입니다. fault suffix의 guest stack window에서도
`guest_stack_m4=0x000000FF`가 확인되어, `RET`가 읽은 source가 guest arena
밖의 `0x000000FF`임을 확정했습니다.

실행 파일 심볼과 disassembly는 다음을 확인했습니다.

```text
00000000402ad353 T RepiuLinuxX64ReturnThunk
00000000402ad3dd T RecoverGuestStackException
00000000402ad3df T RecoverHostStackException

402ad3dc: cc                    int3
402ad3dd <RecoverGuestStackException>:
402ad3dd: 0f 0b                 ud2
```

따라서 `0x402AD3DC`는 resolver target이 0일 때의
`RepiuLinuxX64ReturnThunk` fail-closed sentinel이고, 이어지는
`0x402AD3DD`는 `RecoverGuestStackException`의 x64 fail-closed `UD2`입니다.
이번 종료는 guest `UD2` 실행이나 far jump 변환 실패가 아닙니다.

`REPIU_DPMI_1E7F_PROBE_SUCCESS=1`은 여전히 CF만 조정하는 진단용 경로입니다.
이 경로가 만든 성공 분기의 반환 frame이 실제 `1E7Fh` private ABI와 일치한다고
확인되지 않았으므로, `0xFF`를 무시하거나 임의의 target으로 치환하는 코드는
추가하지 않았습니다.

## 검증

Linux x64 재빌드:

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
cmake --build build/linux_x64_debug --target repiu_core_probe -j 2
[100%] Built target repiu_core_probe
```

core probe:

```text
far_jump_selector_translation=true,valid=true,missing=true,out_of_limit=true
core_probe_total=22
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

runtime:

```text
wsl.exe -d Ubuntu-24.04 -- bash -lc "cd /mnt/e/MYWORK/Projects/rePIU && REPIU_DOS_INT_TRACE=1 REPIU_DPMI_1E7F_TRACE=1 REPIU_DPMI_1E7F_PROBE_SUCCESS=1 REPIU_LINUX_X64_RETURN_TRACE=1 REPIU_GUEST_WATCH=0x010F0232 timeout -k 1s 5s ./build/linux_x64_debug/repiu pumpit2a"
```

핵심 runtime trace:

```text
[repiu-guest-int3] #1 eip=0x01100042 ...
[repiu-watch] event=step guest=0x010F0232 n=1 at=0x010F0232 le_bytes=0x0000C35D5F5E5B07 ...
[repiu-x64-return] result=translation-failed source=0x000000FF cache=0x00000000 detail=dynamic AOT target is outside the guest arena
[repiu-fault] unhandled signal=0x5 rip=0x402ad3dd eip=0x402ad3dc ... guest_stack_m4=0xff ...
[repiu-fault] unhandled signal=0x4 rip=0x402ad3dd eip=0x402ad3dd ...
```

## 상태 및 후속 작업

* `66 EA` far jump HLE와 selector translation: 완료
* `0x01100042` guest `INT3` 소비: 완료
* `0x010F0232`의 invalid return source provenance: 확정
* AOT `INT3 → RecoverGuestStackException UD2` 분류: 확정
* `INT 31h AX=1E7Fh` 실제 성공 ABI: 미확정

다음 구현은 원본 바이너리에서 `1E7Fh` 성공 시 반환 frame을 생성하는 코드 또는
그 frame의 upstream writer를 확인한 뒤에 진행해야 합니다.

---

# Work log 20260905-602 — Linux x64 unresolved return-address provenance

Design: [20260905-602](../design/20260905-602-linux-x64-unresolved-return-provenance.md)  
Work order: [20260905-602](../work-orders/20260905-602-linux-x64-unresolved-return-provenance.md)

## Result

The post-Task-601 frontier was reproduced with the probe-success runtime.
Guest `INT3` at `0x01100042` was consumed once, and the former far-jump SIGILL
at `0x010F016B` did not recur.

The watch at `0x010F0232` produced:

```text
[repiu-watch] event=step guest=0x010F0232 n=1 at=0x010F0232
  le_bytes=0x0000C35D5F5E5B07 ... esp=0x0158CC58
[repiu-x64-return] result=translation-failed source=0x000000FF
  cache=0x00000000 detail=dynamic AOT target is outside the guest arena
```

The little-endian bytes `07 5B 5E 5F 5D C3` are `POP ES`, `POP EBX`,
`POP ESI`, `POP EDI`, `POP EBP`, and `RET`. The guest stack window in the fault
suffix also reports `guest_stack_m4=0x000000FF`, confirming that `RET` consumed
`0x000000FF`, outside the guest arena.

Executable symbols and disassembly confirmed:

```text
00000000402ad353 T RepiuLinuxX64ReturnThunk
00000000402ad3dd T RecoverGuestStackException
00000000402ad3df T RecoverHostStackException

402ad3dc: cc                    int3
402ad3dd <RecoverGuestStackException>:
402ad3dd: 0f 0b                 ud2
```

Therefore `0x402AD3DC` is the `RepiuLinuxX64ReturnThunk` fail-closed sentinel
when the resolver target is zero, and `0x402AD3DD` is the x64 fail-closed `UD2`
in `RecoverGuestStackException`. The termination is not guest `UD2` execution
and is not a far-jump translation failure.

`REPIU_DPMI_1E7F_PROBE_SUCCESS=1` remains a diagnostic path that only adjusts
CF. Its success-path return frame has not been established as the real private
`1E7Fh` ABI, so no code was added to ignore `0xFF` or substitute an arbitrary
target.

## Verification

Linux x64 rebuild:

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
cmake --build build/linux_x64_debug --target repiu_core_probe -j 2
[100%] Built target repiu_core_probe
```

Core probe:

```text
far_jump_selector_translation=true,valid=true,missing=true,out_of_limit=true
core_probe_total=22
core_probe_failures=0
core_probe_all=true
core_probe_skipped=2 stack_bridge guest_stack_switch
```

The runtime command and key trace are recorded in the Korean section above.

## Status and follow-up

* `66 EA` far-jump HLE and selector translation: complete
* Guest `INT3` consumption at `0x01100042`: complete
* Invalid return-source provenance at `0x010F0232`: confirmed
* AOT `INT3 -> RecoverGuestStackException UD2` classification: confirmed
* Actual success ABI of `INT 31h AX=1E7Fh`: unresolved

The next implementation should begin only after the original binary identifies
the code that creates the `1E7Fh` success return frame or its upstream writer.
