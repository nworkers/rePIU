# 20260801-392 Hybrid Segment-Override Dispatch 작업 로그 / Work Log

설계: [20260801-392-hybrid-segment-override-dispatch.md](../design/20260801-392-hybrid-segment-override-dispatch.md)

작업 지시: [20260801-392-hybrid-segment-override-dispatch.md](../work-orders/20260801-392-hybrid-segment-override-dispatch.md)

## 한국어

### 구현

- 기존 selector-guard native slot 뒤에 fail-closed HLE dispatch companion slot을 함께 방출합니다.
- live segment resolution이 `NativeFolded`이면 native entry를 복원하고, `HleLowMemory`이면 companion slot으로 `JMP rel32`를 패치하며, unresolved이면 기존 `INT3`를 유지합니다.
- native guard의 실행 중 selector mismatch도 companion slot으로 보내며, 지원하지 않는 명령은 기존 INT3/VEH fallback으로 복구합니다.
- 정적 배치, 동적 append offset, coverage validator와 synthetic native/HLE/unresolved 전환 probe를 갱신했습니다.
- 구현은 PIU 주소나 게임 상태를 하드코딩하지 않고 명령 분류와 live segment policy만 사용합니다.

### 검증

- Debug와 Release의 `repiu_loader_win32`, `repiu_aot_probe` 빌드가 성공했습니다. 기존 C4819/LNK4217 경고만 남았습니다.
- 두 PIU 실행 파일 구성의 Debug/Release 전체 probe가 종료 코드 0이며 `segment_override_dispatch_specific=true`, `segment_override_hybrid_patch=true`, `selector_guard_all=true`, `coherence_all=true`입니다.
- Release 3초 OFF/ON은 아직 frame 단계 전이므로 frame 정규화에는 쓰지 않았습니다. 같은 wall-clock에서 전체 예외는 `35,522 -> 27,128`(-23.63%), access violation은 `11,293 -> 2,119`(-81.24%), VEH cycles는 `4.725B -> 2.941B`(-37.77%), guest-run cycles는 `11.217B -> 11.172B`(-0.40%)였습니다.
- Glide direct entry는 `1,984 -> 13,380`으로 ON 쪽이 더 진행했고, 양쪽 모두 target miss/terminal 0, timeout 종료, final exception 없음입니다.
- ON dispatch entry/attempt/success/fallback은 `14,284/14,284/5,795/8,489`, fallback reason은 VEH-required 1/unhandled 8,488이며 invalid-site/target/state/unknown은 0입니다. native 경로의 effective `88/89/8C` 계수는 OFF와 사실상 동일하여 broad routing이 제거됐음을 확인했습니다.

### 판정

Task 391의 broad dispatch는 장시간 로그에서 명확한 회귀로 기각했습니다. 이번 hybrid 경로는 구조 검증과 짧은 Release 실행에서는 안전성과 진행 개선 신호가 있지만, 반복 unhandled가 장면 고정 비용인지 frame 비례 비용인지 장시간 Music Select 캡처로 확인하기 전까지 기본값은 OFF로 유지합니다.

## English

### Implementation

- Emit a fail-closed HLE-dispatch companion slot after the existing selector-guard native slot.
- Live segment resolution restores the native entry for `NativeFolded`, patches a `JMP rel32` to the companion for `HleLowMemory`, and retains `INT3` for unresolved state.
- A runtime selector mismatch in the native guard also enters the companion; unsupported instructions recover through the existing INT3/VEH fallback.
- Updated static placement, dynamic-append offsets, coverage validation, and a synthetic native/HLE/unresolved transition probe.
- The implementation contains no PIU addresses or game-state checks; it depends only on instruction classification and live segment policy.

### Verification

- Debug and Release builds of `repiu_loader_win32` and `repiu_aot_probe` passed with only pre-existing C4819/LNK4217 warnings.
- Full Debug/Release probes for both PIU layouts exited zero with `segment_override_dispatch_specific=true`, `segment_override_hybrid_patch=true`, `selector_guard_all=true`, and `coherence_all=true`.
- The three-second Release OFF/ON runs did not yet reach frames and were not frame-normalized. In the same wall-clock interval, total exceptions fell `35,522 -> 27,128` (-23.63%), access violations `11,293 -> 2,119` (-81.24%), VEH cycles `4.725B -> 2.941B` (-37.77%), and guest-run cycles `11.217B -> 11.172B` (-0.40%).
- Glide direct entries rose `1,984 -> 13,380`, indicating more progress when enabled. Both runs had zero target misses/terminal failures, stopped at timeout, and had no final exception.
- Enabled dispatch entry/attempt/success/fallback was `14,284/14,284/5,795/8,489`. Fallback reasons were one VEH-required and 8,488 unhandled, with zero invalid-site, target, state, or unknown failures. Effective native `88/89/8C` counts remained essentially identical to OFF, confirming broad routing was removed.

### Decision

Task 391 broad dispatch is rejected by the long-run regression. The hybrid path passes structural validation and shows safe progress in the short Release run, but default remains OFF until a long Music Select capture determines whether repeated unhandled cases are fixed scene cost or frame-scaled cost.

## 장시간 후속 검증

- 올바른 `pumpit1` hybrid 로그는 약 43.84초 동안 3,087 frame, Task 390 기준은 약 44.16초 동안 3,914 frame으로 처리량이 21.13% 감소했습니다.
- frame당 single-step +24.16%, breakpoint +26.44%, 전체 예외 +18.47%, guest-run cycles +25.49%, VEH cycles +47.50%, Glide gate cycles +49.60%입니다. access violation만 21.13% 감소했습니다.
- dispatch entry/attempt/success/fallback은 `53,924/53,924/32,503/21,421`이며 fallback 중 21,420건이 unhandled였습니다. final exception과 Glide miss/terminal은 0이었습니다.
- hybrid 기본 승격을 기각하고 환경 변수 기본값을 OFF로 유지합니다. opt-in 구현은 후속 명령군 분류를 위한 진단 경로로만 남깁니다.
- 무음이 발생한 별도 로그는 `piu_1st` 대상이었고 `MSCDEX available/audio=false/false`였습니다. 제가 제공한 명령에서 `pumpit1` 인자를 누락한 것이 원인이며, 올바른 hybrid `pumpit1` 로그는 `true/true`, 51 tracks, 41 requests로 오디오 요청이 정상입니다.

## Long-run follow-up

- The correct `pumpit1` hybrid log processed 3,087 frames in about 43.84 seconds versus 3,914 in about 44.16 seconds for Task 390, a 21.13% throughput loss.
- Per frame, single steps rose 24.16%, breakpoints 26.44%, total exceptions 18.47%, guest-run cycles 25.49%, VEH cycles 47.50%, and Glide-gate cycles 49.60%. Only access violations fell, by 21.13%.
- Dispatch entry/attempt/success/fallback was `53,924/53,924/32,503/21,421`, with 21,420 unhandled fallbacks. Final exception and Glide miss/terminal remained zero.
- Hybrid default promotion is rejected. The environment-variable default remains OFF, and the opt-in implementation remains only as a diagnostic path for later instruction-family analysis.
- The separate silent log targeted `piu_1st` and reported `MSCDEX available/audio=false/false`. The cause was my omission of the `pumpit1` command argument; the correct hybrid `pumpit1` log reported `true/true`, 51 tracks, and 41 requests.
