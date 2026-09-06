# 20260906-614 Linux x64 high-byte source 재인코딩 작업 로그

## 한국어

### 결과

Linux x64 AOT에서 `ESP` 메모리 피연산자와 `AH/CH/DH/BH` source가 함께 있는
명령을 host `SPL/BPL/SIL/DIL`로 잘못 해석하지 않도록 수정했습니다. source-only
ModRM high-byte 형식만 새 lowering으로 허용하고, destination 또는 read/write
형식은 계속 fail-closed refusal로 남겼습니다.

새 lowering은 source GPR의 low/high byte를 legacy `XCHG`로 교환한 뒤 `R14B`에
복사하고 원래 GPR을 복원합니다. 이후 원래 명령의 ModRM `reg`와 `ESP` base를
각각 `R14B`와 `R15`로 재인코딩합니다. `XCHG`와 `MOV`는 EFLAGS를 변경하지
않으므로 guest flags와 source GPR을 보존합니다.

### 검증

* Linux x64 `repiu_core_probe`: `core_probe_total=24`,
  `core_probe_failures=0`, `core_probe_all=true`.
* byte-level probe: `mov byte ptr [esp],ah`가
  `86 C4 41 88 C6 86 C4 45 88 34 27`로 lowering됩니다.
* synthetic execution probe: guest stack에 `AH=0xC3`가 기록되고,
  `ZF=1`이 유지됩니다.
* AOT image decode 검증: `decode_failures=0`.
* `pumpit2a`: 이전 high-byte 오염 지점에서 중단하지 않고 이후 guest
  `PUSH ES` 경계까지 진행했습니다. 다음 관찰 지점은 x64 return resolver의
  `source=0` fail-closed sentinel입니다.

### 다음 작업

`PUSH ES`는 별도 진단 작업에서 shared HLE handler가 `handled=1`로 처리되는
것을 확인했습니다. 현재 남은 Linux x64 문제는 반환 thunk가 유효한 guest
return source를 받지 못하는 이유를 특정하는 것입니다.

## English

### Result

The Linux x64 AOT lowering no longer lets an instruction combining an `ESP`
memory operand with an `AH/CH/DH/BH` source name host `SPL/BPL/SIL/DIL`. Only
source-only ModRM high-byte forms are admitted by the new lowering; destination
and read/write forms remain fail-closed refusals.

The lowering exchanges the source GPR's low and high bytes using legacy
`XCHG`, copies the original high byte into `R14B`, and restores the source GPR.
It then re-encodes the original operation with `R14B` as the ModRM register and
`R15` as the former `ESP` base. `XCHG` and `MOV` do not modify EFLAGS, so the
guest flags and source GPR are preserved.

### Verification

* Linux x64 `repiu_core_probe`: `core_probe_total=24`,
  `core_probe_failures=0`, `core_probe_all=true`.
* Byte-level probe: `mov byte ptr [esp],ah` lowers to
  `86 C4 41 88 C6 86 C4 45 88 34 27`.
* Synthetic execution probe: guest stack memory receives `AH=0xC3`, and
  `ZF=1` remains set.
* AOT image decode validation: `decode_failures=0`.
* `pumpit2a` proceeds past the former high-byte corruption point and reaches a
  later guest `PUSH ES` boundary. The next observed point is the x64 return
  resolver's `source=0` fail-closed sentinel.

### Next task

A separate diagnostic task confirmed that `PUSH ES` is handled by the shared
HLE handler with `handled=1`. The remaining Linux x64 issue is to determine why
the return thunk receives no valid guest return source.
