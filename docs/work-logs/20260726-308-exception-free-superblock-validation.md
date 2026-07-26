# 20260726-308 작업 로그: exception-free superblock 아키텍처 검증 / Work log: exception-free superblock architecture validation

설계: [20260726-308-exception-free-superblock-validation.md](../design/20260726-308-exception-free-superblock-validation.md)

작업 지시: [20260726-308-exception-free-superblock-validation.md](../work-orders/20260726-308-exception-free-superblock-validation.md)

## 한국어

### 구현

기존 planner/emitter가 direct call/jump, conditional branch, fallthrough, backward edge를
이미 cache-local로 연결한다는 사실을 확인했습니다. 따라서 새 기본 블록 형식을 만들지
않고 `REPIU_AOT_DBT_SUPERBLOCK=1|on|true`에서 planner-HLE `INT3` slot만 정상
host-call 경계로 바꾸었습니다.

Win32 x86 thunk는 `pushfd/pushad`, `fxsave/fxrstor`, host ESP와 TIB stack
base/limit 전환을 사용합니다. 공용 `DispatchGuestHleInstruction`을 호출한 뒤 활성
cache target이 있으면 직접 복귀합니다. target miss는 HLE side effect를 다시 실행하지
않도록 처리된 다음 guest EIP에서 기존 TF bridge를 사용합니다. invalid site, 미처리
명령과 사전 제외 명령은 상태를 바꾸기 전에 planner-HLE `INT3`로 fail-closed합니다.

site metadata, slot layout, placement coverage와 thunk 존재를 `repiu_aot_probe`에
추가했습니다. 종료 계측은 dispatch `entry/attempt/success/fallback`, 원인별 fallback,
마지막 성공 source/next/bytes를 출력합니다.

### 안전성 발견

초기 30초 ON은 약 19.6초에 `0x03042EBE`의 `call dword ptr [0x032D9C90]`에서
`0xFFFFFFFF`를 호출하며 AV가 났습니다. 마지막 직접 HLE는
`0x030F5D27: CD 21`이었습니다. 이는 `INT 21h AH=25h`로 INT 8 vector를 등록하는
경로입니다.

직접 경로는 INT 8 selector를 `0023:03042EAE`로 저장했지만 기존 OFF 경로는
`002B:03042EAE`를 저장했습니다. `INT/IRET` 계열에는 현재 VEH/native segment
계약이 포함되어 있으며 일반 register/flags-only host call로 대체할 수 없다는
증거입니다. 첫 안전 slice에서는 `INT`, `INT1`, `INT3`, `INTO`, `IRET`, `IRETD`,
`IRETQ`와 segment/ESP write를 모두 기존 VEH 경계에 남겼습니다.

### 검증

- Win32 x86 Debug 전체 빌드: 성공.
- `repiu_aot_probe build/runtime_mounts/pumpit1/PIU/PIU.EXE`: 성공.
  `superblock_hle_dispatch_ready/layout/coverage/placement=true`,
  `selector_guard_all=true`, `linear_span_all=true`, `coherence_all=true`.
- 안전 slice 30초 ON: 정상 timeout, exception 0, legacy fallback 0,
  progress 18,354, single-step 113,531, 직접 HLE success 25,134.
- 동일 빌드 60초 OFF/ON: 양쪽 모두 정상 timeout, exception 0, legacy fallback 0,
  Glide gate 4,582/4,582, Glide gap count 756 및 unique 5로 동일.
- 두 EEPROM SHA-256:
  `A1FC1D120EF12DE4FB3608551750F93E02F911F26A3DDF9054ABCE4846652570`.

| 60초 지표 | OFF | ON | 변화 |
|---|---:|---:|---:|
| progress | 44,977 | 45,716 | +1.64% |
| single-step | 276,680 | 254,889 | -7.88% |
| AOT boundary | 66,245 | 41,224 | -37.77% |
| AOT re-entry | 73,592 | 73,708 | +0.16% |
| HLE 직접 성공 | 0 | 25,134 | +25,134 |
| HLE fallback | 0 | 19,196 | +19,196 |

ON의 fallback은 sensitive/VEH-required 14,458회와 unhandled 4,738회이며
`attempt = success + fallback = 44,330`을 만족합니다. target/state/unknown은 모두
0입니다.

### 결론

정상 host-call HLE 경계의 정확성과 구현 가능성은 확인했습니다. 그러나 planner-HLE
25,134회를 예외 없이 처리하고 AOT boundary를 37.77% 줄여도 whole-run progress는
`1.0164x`에 불과했습니다. 5배 go/no-go에는 약 4.92배, 60배 목표에는 약 59.03배의
추가 배율이 남아 있어 기준과 질적으로 다른 결과입니다.
일반 HLE 예외 제거를 다음 주 성능 아키텍처로 확장하지 않습니다. 다음 단계는 예외
개수가 아니라 실제 CPU 시간을 guest address와 provenance별로 계측하는 것입니다.

사용자 소유 `repiu_log.txt`는 수정하거나 staging하지 않았습니다.

## English

Confirmed that the existing planner and emitter already chain direct calls/jumps,
conditional branches, fallthroughs, and backward edges within the cache. Task 308 therefore
adds no second block format. Under `REPIU_AOT_DBT_SUPERBLOCK=1|on|true`, it replaces ordinary
planner-HLE `INT3` slots with a normal host-call boundary.

The Win32 x86 thunk preserves GPR/EFLAGS and x87/MMX/SSE state, switches ESP and TIB stack
bounds to the host stack, calls the shared HLE chain, and resumes at an active cache target.
A target miss resumes the already-handled next guest EIP through the established TF bridge;
pre-dispatch exclusions and failures retain the original planner-HLE `INT3`.

The first unrestricted run exposed a semantic dependency: direct `INT 21h AH=25h` registered
INT 8 as `0023:03042EAE`, while the established OFF path registered `002B:03042EAE`. The
changed selector later produced an AV through an invalid interrupt target. The safe first
slice therefore keeps all `INT/IRET` and segment/ESP-writing instructions VEH-mediated.

The full Win32 x86 Debug build and all AOT probes passed. A controlled 60-second OFF/ON pair
completed with no exception or legacy fallback, identical 4,582 Glide gates, identical Glide
gap counts, and matching EEPROM hashes. ON directly handled 25,134 HLE sites and reduced
single-step by 7.88% and AOT boundaries by 37.77%, but progress increased only from 44,977 to
45,716, or 1.64%. This fails the 5x architecture gate by a wide margin and rejects ordinary
HLE exception removal as the missing path to the 60x target. The next experiment must measure
actual CPU time by guest address and provenance.
