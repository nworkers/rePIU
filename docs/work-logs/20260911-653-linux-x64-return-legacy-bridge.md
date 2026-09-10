# 20260911-653 작업 로그: Linux x64 unresolved return legacy bridge

설계: [20260911-653 설계](../design/20260911-653-linux-x64-return-legacy-bridge.md)  
작업 지시: [20260911-653 작업 지시](../work-orders/20260911-653-linux-x64-return-legacy-bridge.md)  
분석: [linux-port-frontier 3.90](../analysis/linux-port-frontier.md)

## 한국어

### 결과

1. Linux x64 return resolver가 cache 해석에 실패했을 때 guest target의 첫 명령을
   공용 `ClassifyLongModeBytes`로 검사하도록 했습니다.
2. guest arena 안의 `kIdenticalBytes` 명령에만 전용 legacy-resume thunk를 허용했습니다.
   zero, 범위 밖, stack/segment 등 의미가 달라질 수 있는 명령은 기존 fail-closed를
   유지합니다.
3. thunk는 저장된 guest EFLAGS에 TF를 더해 복원하고 guest EAX를 복원한 뒤 frame의
   `guest_continuation`으로 이동합니다.
4. Linux x64 Debug 증분 빌드와 전체 core probe가 통과했습니다.
   `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`였습니다.
5. 실제 `pumpit2a`에서 `0x010F925D`에 대한 `legacy-fallback` 선택을 확인했고 기존
   segment-override 번역 경계를 넘어 실행이 계속됐습니다.

### 검증 로그

```text
general_stack_...,legacy_resume_policy=true,legacy_resume_thunk=true,...
core_probe_total=27
core_probe_failures=0
core_probe_all=true
[repiu-x64-return] result=legacy-fallback source=0x010F925D cache=0x402BD957 producer=0x010F1E56 guest_esp=0x0158CC54 detail=byte-identical first instruction
```

### 남은 경계

게임은 아직 정상 실행되지 않습니다. bridge 이후 같은 `0x010F1E56` RET가 다시
실행되어 post-pop guest ESP `0x0158CC6C`에서 zero target을 소비합니다. watch 결과
`0x010F9273`은 실행되지 않았고 `0x010F9258`은 한 번만 확인됐습니다. 다음 작업은 두
번째 epilogue에 도달하는 실제 진입 경로와 guest stack frame을 규명하는 것입니다.

## English

### Result

1. The Linux x64 return resolver now classifies the first instruction at an
   unresolved guest target with the shared `ClassifyLongModeBytes` policy.
2. Only a `kIdenticalBytes` instruction inside the guest arena may use the
   dedicated legacy-resume thunk. Zero, out-of-range, stack, segment, and other
   potentially divergent instructions keep the existing fail-closed behavior.
3. The thunk restores saved guest EFLAGS with TF set, restores guest EAX, and
   jumps to the frame's `guest_continuation`.
4. The incremental Linux x64 Debug build and all core probes passed:
   `core_probe_total=27`, `core_probe_failures=0`, `core_probe_all=true`.
5. A real `pumpit2a` run selected `legacy-fallback` for `0x010F925D` and continued
   beyond the former segment-override translation boundary.

### Verification log

```text
general_stack_...,legacy_resume_policy=true,legacy_resume_thunk=true,...
core_probe_total=27
core_probe_failures=0
core_probe_all=true
[repiu-x64-return] result=legacy-fallback source=0x010F925D cache=0x402BD957 producer=0x010F1E56 guest_esp=0x0158CC54 detail=byte-identical first instruction
```

### Remaining frontier

The game still does not run normally. After the bridge, the same `0x010F1E56`
RET executes again and consumes a zero target at post-pop guest ESP `0x0158CC6C`.
The `0x010F9273` watch did not fire and `0x010F9258` fired only once. The next task
must identify the real entry path and guest stack frame leading to this second
epilogue.
