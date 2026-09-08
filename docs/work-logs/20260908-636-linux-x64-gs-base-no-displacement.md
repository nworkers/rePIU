# Task 636 작업 로그: Linux x64 GS base/no-displacement segment override

설계: [20260908-636](../design/20260908-636-linux-x64-gs-base-no-displacement.md) ·
작업 지시: [20260908-636](../work-orders/20260908-636-linux-x64-gs-base-no-displacement.md) ·
분석: [linux-port-frontier 3.73](../analysis/linux-port-frontier.md)

## 한국어

### 작업 결과

`LongModeSegmentOverrideEmittable`이 GS와 non-SIB `mod=00`
base/no-displacement 형식을 허용하도록 확장했습니다. FS는 계속 거부합니다.
새 형식은 ModRM을 `mod=10`으로 넓히고 `original_displacement=0`인 disp32
slot을 만들어 기존 patcher가 live GS base를 접도록 합니다. emitted access에서
GS prefix를 제거하므로 host GS는 사용하지 않습니다.

변위가 없는 경우 원본 suffix를 ModRM 바로 뒤에서 복사하도록 별도 처리했습니다.
이 규칙은 `65 80 38 6A`의 immediate `0x6A`를 prefix나 원본 명령 전체와
혼동하지 않게 합니다. absolute 여부도 `mod=00 && rm=5`로 좁혀 새 EAX base
형식이 no-base SIB로 잘못 변환되지 않게 했습니다.

### probe

Linux x64 guest-register probe는 네 segment slot을 한 image에서 실행합니다.

* 실제 frontier CMP `65 80 38 6A`
* 실제 frontier MOV `65 8A 18`
* 기존 ES base+disp8
* 기존 ES absolute disp32

selector 일치, 첫 GS guard 불일치, HLE policy 전환, native 복원 경로가 모두
통과했습니다. FS, SIB, base+disp32 거부도 별도로 확인합니다.

```text
segment_patcher_sites observed=0x4 expected=0x4
segment_refusals_kept observed=0x1 expected=0x1
segment_patcher_native observed=0x4 expected=0x4
segment_gs_compare_equal observed=0x1 expected=0x1
segment_gs_access_value observed=0x6a expected=0x6a
segment_guard_boundary observed=0x1 expected=0x1
segment_hle_routed observed=0x4 expected=0x4
segment_restore_native observed=0x4 expected=0x4
guest_segment_override=true slots=4
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

### census

```text
emittable           51184  (98.69%)
refused             682  (1.31%)
blocks complete     11174  (89.33%)
emitter counters    ... segments=64 ... refused=682  agrees=true
kSegmentOverrideMem                    1  (0.30% of non-copy)
```

Task 634 기준보다 emittable과 complete block이 각각 56개 늘고 refusal이 56개
줄었습니다. emitted segment override는 7개에서 64개로 늘었습니다.

### 실제 실행

세 번의 `pumpit2a` 실행에서 기존 `0x010F06D0` GS fault는 재발하지 않았습니다.
모두 다음 `0x010F1E17`에서 멈췄습니다.

```text
[repiu-fault] unhandled signal=0xb rip=0x10f1e17 eip=0x10f1e17
  access=<실행마다 달라짐> bytes=e8 cc 31 00 00 ... esp=0x158cc68
```

이 명령은 `CALL 0x010F4FE8`입니다. map trace는 source가 cache
`0x20002841`에 16바이트 lowering으로 존재하고 target fixup이 `resolved=1`이며,
target도 cache `0x200028B5`에 있음을 확인했습니다. 그러나 fault의 `rip==eip`은
실제로 실행된 것이 cache lowering이 아니라 guest 주소의 raw 64비트 CALL임을
보여 줍니다.

### 판단

Task 636의 GS 의미와 기존 guard/patch 계약은 실행 검증됐고 이전 frontier를
해소했습니다. 게임은 아직 정상 실행되지 않습니다. 다음 작업은 direct-call
emitter나 fixup을 다시 만드는 것이 아니라, 어떤 boundary/reentry 경로가 유효한
cache entry를 건너뛰고 `0x010F1E17`을 guest 주소 단일 실행으로 보냈는지
측정하는 것입니다.

## English

### Result

Extended `LongModeSegmentOverrideEmittable` to admit GS and the non-SIB
`mod=00` base/no-displacement form. FS remains refused. The new form widens
ModRM to `mod=10`, creates a disp32 slot with `original_displacement=0`, and
lets the existing patcher fold the live GS base. The emitted access removes the
GS prefix and therefore does not use host GS.

No-displacement instructions copy their original suffix from immediately after
ModRM. This keeps the `0x6A` immediate in `65 80 38 6A` from being confused with
the prefix or duplicated with the original instruction. Absolute-form
detection is now specifically `mod=00 && rm=5`, preventing the new EAX-base
form from being rewritten as a no-base SIB.

### Probe

The Linux x64 guest-register probe executes four segment slots in one image:

* the actual frontier CMP `65 80 38 6A`;
* the actual frontier MOV `65 8A 18`;
* the existing ES base+disp8 form; and
* the existing ES absolute-disp32 form.

Matching selector, first-GS-guard mismatch, HLE policy routing, and native
restoration all pass. The probe separately confirms continued refusal of FS,
SIB, and base+disp32.

```text
segment_patcher_sites observed=0x4 expected=0x4
segment_refusals_kept observed=0x1 expected=0x1
segment_patcher_native observed=0x4 expected=0x4
segment_gs_compare_equal observed=0x1 expected=0x1
segment_gs_access_value observed=0x6a expected=0x6a
segment_guard_boundary observed=0x1 expected=0x1
segment_hle_routed observed=0x4 expected=0x4
segment_restore_native observed=0x4 expected=0x4
guest_segment_override=true slots=4
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

### Census

```text
emittable           51184  (98.69%)
refused             682  (1.31%)
blocks complete     11174  (89.33%)
emitter counters    ... segments=64 ... refused=682  agrees=true
kSegmentOverrideMem                    1  (0.30% of non-copy)
```

Compared with Task 634, emittable instructions and complete blocks each rise
by 56 while refusals fall by 56. Emitted segment overrides rise from 7 to 64.

### Runtime

Three `pumpit2a` runs passed the former GS fault at `0x010F06D0`. All stopped at
the same next address, `0x010F1E17`.

```text
[repiu-fault] unhandled signal=0xb rip=0x10f1e17 eip=0x10f1e17
  access=<varies by run> bytes=e8 cc 31 00 00 ... esp=0x158cc68
```

The instruction is `CALL 0x010F4FE8`. Map tracing confirms that the source has
a 16-byte lowering at cache `0x20002841`, its target fixup is `resolved=1`, and
the target exists at cache `0x200028B5`. However, `rip==eip` at the fault proves
that execution used the raw 64-bit CALL at the guest address rather than the
cache lowering.

### Assessment

Task 636's GS semantics and the existing guard/patch contract are
execution-tested, and the old frontier is resolved. The game still does not
run normally. The next task should not rebuild the direct-call emitter or
fixup; it should measure which boundary or reentry path skipped a valid cache
entry and sent `0x010F1E17` through native single-instruction execution.
