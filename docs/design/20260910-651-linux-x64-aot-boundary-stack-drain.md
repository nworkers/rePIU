# 20260910-651 설계: Linux x64 legacy stack run 완결

## 한국어

### 문제

간접 CALL `0x010F4ACF -> 0x010F920C`의 반환 주소는 이제 guest stack에 정상적으로
기록됩니다. 그러나 대상 prologue `PUSH EBX/ECX/EDX/ESI/EDI/ES/FS/GS/EBP;
SUB ESP,4`가 x64 legacy fallback에서 실행될 때 연속 stack HLE는 PUSH까지만
처리하고 `SUB ESP,4`에서 멈춥니다. 남은 원본 바이트가 long mode에서 실행되면 guest
ESP가 아니라 host RSP가 감소합니다.

그 결과 epilogue의 저장 슬롯이 한 칸 어긋납니다. `ADD ESP,4; POP EBP` 뒤의 guarded
segment POP가 잘못 정렬된 selector를 보게 되고, 마지막 `RET 0x010F1E56`은 반환 주소
대신 0을 소비합니다. `POP GS`에 진단 sentinel을 재설치하여 HLE 경계로 강제하면 segment와
일반 POP 연속 처리가 정상 작동한다는 점도 확인했습니다.

### 설계

1. x64 legacy stack-run helper가 일반/segment PUSH·POP에 이어지는 직접형
   `SUB ESP, imm8`와 `SUB ESP, imm32`를 guest ESP에 적용합니다.
2. SUB는 32-bit wraparound 연산으로 수행하고 CF/PF/AF/ZF/SF/OF를 기존 공용
   `SetCompareFlags`로 갱신합니다.
3. segment HLE가 원본 guest EIP를 전진시킨 경우에는 AOT 상태 flag와 무관하게 최대
   16개 명령의 bounded stack drain을 허용합니다. opcode `0F` 경로는 PUSH뿐 아니라
   POP FS/GS도 시도합니다.
4. 다른 ESP arithmetic, 메모리 피연산자, RET는 이 helper의 범위를 넓히지 않습니다.

```mermaid
flowchart LR
    A[legacy fallback prologue] --> B[PUSH run을 guest ESP에 적용]
    B --> C[SUB ESP immediate를 guest ESP에 적용]
    C --> D[AOT cache body]
    D --> E[정렬된 epilogue POP]
    E --> F[RET가 원래 반환 주소 소비]
```

### 검증 전략

합성 probe에서 general/segment PUSH 뒤 `SUB ESP,4`까지 한 번의 bounded run으로 처리하고
ESP와 산술 flag를 확인합니다. 상태 flag가 없는 segment/general POP epilogue도 기존처럼
검증합니다. Linux x64 Debug 빌드와 전체 core probe 후 실제 `pumpit2a`에서
`0x010F1E56`이 `0x010F4AD1`로 반환하는지 확인합니다.

## English

### Problem

The indirect call `0x010F4ACF -> 0x010F920C` now writes its return address to
the guest stack correctly. Its target prologue is `PUSH EBX/ECX/EDX/ESI/EDI/
ES/FS/GS/EBP; SUB ESP,4`, however, and the x64 legacy-fallback stack run stops
after the PUSH instructions. Executing the remaining `SUB ESP,4` byte in long
mode adjusts host RSP instead of guest ESP.

This shifts the saved-slot layout by one dword. After `ADD ESP,4; POP EBP`, the
guarded segment POPs observe misaligned selectors, and final `RET 0x010F1E56`
consumes zero instead of the return address. A diagnostic sentinel that forces
`POP GS` through the HLE boundary also confirmed that consecutive segment and
general POP draining itself works.

### Design

1. Extend the x64 legacy stack-run helper to apply direct `SUB ESP, imm8` and
   `SUB ESP, imm32` following general/segment PUSH or POP operations to guest ESP.
2. Use 32-bit wrapping subtraction and update CF/PF/AF/ZF/SF/OF through the
   shared `SetCompareFlags` helper.
3. When segment HLE advances an original guest EIP, allow the bounded 16-entry
   stack drain regardless of AOT state flags. Let the directed `0F` path try
   POP FS/GS as well as segment PUSH.
4. Do not broaden this helper to other ESP arithmetic, memory operands, or RET.

### Verification strategy

Extend the synthetic probe so one bounded run handles general/segment PUSH
followed by `SUB ESP,4`, checking ESP and arithmetic flags. Retain the
flag-independent segment/general POP epilogue case. Build Linux x64 Debug, run
the complete core probe, and then verify that real `pumpit2a` returns from
`0x010F1E56` to `0x010F4AD1`.
