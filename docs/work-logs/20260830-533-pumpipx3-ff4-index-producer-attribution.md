# 20260830-533 pumpipx3 FF /4 index component producer attribution 작업 로그

## 작업 결과

Task 532의 pointer-change 분류를 SIB index/base component 관측으로 확장했습니다. Resolved
32-bit SIB operand에 대해 index/base register 번호와 값을 보존하고, 이전 sample과 비교한
component change count를 site와 hotspot에 누적했습니다. Live reporter에도 해당 상태를
추가했으며, guest state와 AOT dispatch 경로는 변경하지 않았습니다.

## 검증

다음 검증이 통과했습니다.

* Win32 x86 Debug `repiu_aot_probe` build
* Synthetic full probe의 `aot_boundary_opcode_census_all=true`
* Synthetic target attribution의 index/base component assertion
* Linux i386 Release `repiu` build

새 binary로 Task 532와 동일한 trace-free 60초 조건에서 두 title을 측정했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

## 측정 결과

| title/site | index component | pointer/index changes | base component | target read |
| --- | --- | --- | --- | --- |
| `pumpipx3` / `0x010EF6DE` | `ir=2` (EDX), `iv=7` | #1 `21/21`, #3 `27/27`, #4/#5 `121/121` | invalid, `bc=0` | all resolved |
| `pumpit1` / `0x010F1DD7` | `ir=0` (EAX), `iv=3` then `2` | `8/8` then `13/13` | invalid, `bc=0` | all resolved |

`pumpipx3`는 `frames=1486`, `span_ms` 약 56.7초에서 정상 timeout teardown을 완료했고,
`pumpit1`은 `frames=2001`, `span_ms` 약 57.3초에서 동일하게 종료했습니다. 두 실행 모두
target unresolved/truncated/unsupported/unreadable count는 0이었습니다.

## Static correlation

정적 probe에서 pumpipx3 dominant site는 다음 local sequence와 대응했습니다.

```text
0x010EF6CF  mov bl, [eax]
0x010EF6DA  xor edx, edx
0x010EF6DC  mov dl, bl
0x010EF6DE  jmp cs:[edx*4+0x010EF65C]
```

따라서 `mov dl, bl`은 runtime EDX index 7과 일치하는 local producer 후보입니다. 다만
exception boundary snapshot은 직전 guest 명령이 실제로 실행된 경로와 전체 producer chain을
단독으로 증명하지 않습니다.

Pumpit1은 다음 형태입니다.

```text
0x010F1DD5  mov edx, eax
0x010F1DD7  jmp cs:[eax*4+0x010F1D8F]
```

`mov edx, eax`는 사용된 EAX를 쓰지 않으므로 EAX producer는 이번 작업에서 확인하지
못했습니다.

## 결론과 미해결 사항

이번 관측은 두 site의 pointer 변경이 base 변경이 아니라 SIB index 값 변경과 함께 발생함을
확인했습니다. Task 531의 `pumpipx3` same-pointer target-table mutation 해석은 현재 분류
결과로 지지되지 않습니다. 그러나 pumpit1 EAX producer, 실제 guest writer, late drop과의
인과관계는 미해결입니다.

Resolved target instruction의 순수 cycle 비용은 측정하지 않았습니다. 현재 FF sample hook이
VEH boundary 처리 중에 있으므로 해당 timestamp를 target 비용으로 명명하면 kernel, handler,
후속 guest 작업이 섞입니다. 이 분리는 별도 timing boundary 작업으로 남겼습니다.

---

# 20260830-533 Work Log: pumpipx3 FF /4 Index Component Producer Attribution

## Result

Task 532's pointer-change classification was extended with SIB index/base component observation.
Resolved 32-bit SIB operands now retain component register numbers and values, while site and
hotspot state accumulates component-change counts. The live reporter exposes the same state. Guest
state and the AOT dispatch path were unchanged.

## Verification

The following checks passed:

* Win32 x86 Debug `repiu_aot_probe` build
* Synthetic full probe with `aot_boundary_opcode_census_all=true`
* Synthetic index/base component assertions
* Linux i386 Release `repiu` build

Both titles were rerun under Task 532's identical trace-free 60-second conditions:

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

## Measurements

| title/site | index component | pointer/index changes | base component | target reads |
| --- | --- | --- | --- | --- |
| `pumpipx3` / `0x010EF6DE` | `ir=2` (EDX), `iv=7` | #1 `21/21`, #3 `27/27`, #4/#5 `121/121` | invalid, `bc=0` | all resolved |
| `pumpit1` / `0x010F1DD7` | `ir=0` (EAX), `iv=3` then `2` | `8/8` then `13/13` | invalid, `bc=0` | all resolved |

Pumpipx3 completed normal timeout teardown at `frames=1486` and approximately 56.7 seconds;
pumpit1 did so at `frames=2001` and approximately 57.3 seconds. Both runs had zero unresolved,
truncated, unsupported, or unreadable target reads.

## Static correlation

The pumpipx3 dominant site corresponds to:

```text
0x010EF6CF  mov bl, [eax]
0x010EF6DA  xor edx, edx
0x010EF6DC  mov dl, bl
0x010EF6DE  jmp cs:[edx*4+0x010EF65C]
```

`mov dl, bl` is therefore a local producer candidate consistent with runtime EDX index 7. The
exception-boundary snapshot alone does not prove the complete producer chain.

Pumpit1 corresponds to:

```text
0x010F1DD5  mov edx, eax
0x010F1DD7  jmp cs:[eax*4+0x010F1D8F]
```

The preceding instruction writes EDX rather than the EAX used by the SIB operand, so the EAX
producer was not identified in this unit.

## Conclusion and unresolved items

The observation confirms that pointer changes at both sites occur with SIB index-value changes,
not base changes. Task 531's same-pointer pumpipx3 target-table mutation interpretation is not
supported by this classification. The pumpit1 EAX producer, actual guest writer, and causality of
the late drop remain unresolved.

Pure resolved-target instruction cycles were intentionally not measured. The current FF sample
hook is inside VEH boundary handling, so treating its timestamp as target cost would mix kernel,
handler, and subsequent guest work. A dedicated timing boundary remains a separate follow-up.
