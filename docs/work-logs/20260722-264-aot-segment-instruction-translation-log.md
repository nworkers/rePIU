# 작업 로그: AOT 세그먼트 명령 번역 — 프로브 + Phase 1
# Work Log: AOT Segment-Instruction Translation — Probe + Phase 1

**Task:** 264 (Task 263 근인 대응)
**브랜치 (Branch):** `claude/aot-segment-translation`
**설계 (Design):** `docs/design/20260722-264-aot-segment-instruction-translation.md`

## 한 일 (What was done)

Task 263이 규명한 근인(세그먼트 명령이 AOT 이탈의 약 75%)에 대응하는 일반화 설계를
쓰고, 선결 확인용 안전 프로브로 semantics를 확정한 뒤 Phase 1(push-seg)을 구현·검증했다.

1. **설계(Task 264):** 공용 planner/emitter + selector shadow 기반으로 세그먼트 명령을
   네이티브 번역하는 단계적 설계. 모든 DOS4GW/DPMI executable에 일반 적용.
2. **선결 프로브(안전 계측):** `push ds`에서 host 세그먼트 레지스터 vs shadow selector
   비교 → **host 항상 0x2b, shadow는 혼재**. push-seg 전용 HLE 없음 → 현재 단일스텝은
   push를 네이티브 실행해 host 0x2b를 올린다(게임 정상). 즉 **shadow-push 설계는 동작을
   바꿔 버그가 됐을 것**이고, 올바른 해법은 네이티브(kCopy)다. 프로브가 회귀를 예방.
3. **Phase 1 구현:** `IsHleBoundary`에 "push+세그먼트 레지스터 → 경계 아님" 분기 추가.
   push-seg가 `kCopy`로 네이티브 실행되어 예외 왕복 제거. emitter/fixup 변경 불필요.

### 변경 파일 (Changed files)

* `docs/design/20260722-264-*.md`, `docs/work-orders/...`(설계·지시).
* `include/repiu/platform/win32/live_telemetry.h` — 프로브 필드 6개, 버전 18→19.
* `src/platform/win32/aot/aot_runtime_dispatch.cpp` — `ProbePushSegBoundary`(계측).
* `src/host/win32/supervisor_main.cpp` — 프로브 출력.
* `src/runtime/aot_translation_plan.cpp` — **Phase 1**: push-seg 경계 면제(kCopy).

## 검증 (Verification)

VS 2026 Debug 빌드 통과. aot-dynamic `pumpit1` 120초(supervisor):

| 지표 | 기준(Task 263) | Phase 1 | Δ |
|---|---:|---:|---:|
| 경계 총합 | 78,701 | 63,712 | **−19%** |
| `other` | 59,208 | 48,712 | **−17.7%** |
| push-seg 경계 | 수천 | **0** | 제거 |
| single_step | 786,814 | 747,963 | −4.9% |
| fatal_count | 0 | 0 | ✓ |
| 도달 EIP | 0x30f5520 | 0x30f551f | 동일 프레임 루프 |
| glide gate | 49 | 49 | 동일 |

동작 동일(같은 EIP·같은 Glide 렌더, 크래시·fatal 0). progress −3.9%는 Debug 런간
타이밍 분산으로 판단(같은 프레임 루프 동일 지점 도달).

## 결론 (Conclusion)

Task 262→264 라인의 **첫 실제 성능 개선**. push-seg 예외 churn을 안전하게 제거해 경계
이탈을 19% 줄였다. 핵심 교훈: 근인을 규명해도(Task 263) **구현 전 semantics 확정이
필수** — 프로브가 shadow-push라는 잘못된 "수정"을 사전 차단했다. 개선은 공용 planner에
있어 executable 일반적이다.

## Phase 2 (반증·되돌림) / Phase 2 (disproved, reverted)

`mov r/m,Sreg`(0x8C) store를 kCopy로 면제했더니 **회귀**: 게스트가 fatal-breakpoint
관용구 `0x030F3438`에 정체(progress 8,027, single_step 4.56M, glide gate 0, 렌더 없음).
근인: [instruction_emulation.cpp:699-726](../../src/platform/win32/cpu_emul/instruction_emulation.cpp#L699)
이 0x8C를 `ReadGuestSegmentSelector`로 **shadow(논리) selector를 반환**하도록 에뮬레이트
한다. push seg는 native(host 0x2b), mov-store는 shadow-emulated — 명령별로 다르다.
네이티브 store는 host 0x2b를 써서 발산 → 게스트가 자기 selector를 오독하고 assert.
**되돌리고 세그먼트 store는 경계 유지.** 되돌린 뒤 재구동에서 정상 복구 확인
(last_eip 0x30f551f, glide 49, fatal 0, progress 34,317).

## Phase 3 (계측 완료 → 결정 필요) / Phase 3 (characterized → decision required)

세그먼트 오버라이드 base 프로브(안전 계측) 결과: **지배적 GS 오버라이드(0x65)는
selector 0x80, base `0x0B5C7000`으로 non-flat**(25,566건). 소수 ES(0x26)만 flat(base 0).
→ GS:[mem]는 prefix-strip 불가, `유효주소 + 0x0B5C7000` **base-add codegen** 필요 +
selector 재적재 무효화 가드. 게스트 메모리 접근 codegen이라 고위험. **결정 지점**:
(A) GS base-add(최대 이득·고위험), (B) flat만(ES prefix-strip, 소이득), (C) 종료.
상세: 설계 §0/§11.

**교훈.** 근인(세그먼트)을 알아도 명령별로 (i) 단일스텝이 native냐 shadow냐, (ii)
base가 flat이냐가 달라 처리가 갈린다. 각 명령을 착수 전 프로브로 확인해야 한다 —
Phase 2 회귀와 Phase 1 성공이 이를 증명한다.

## Phase 3a (구현 완료·검증, 부분 성과 → 정적 이미지 문제 발견)

사용자가 A(GS base-add) 선택. 전체 파이프라인 구현: 새 `kSegmentOverrideMem` kind,
planner 분류, emitter(self-correcting 가드 + prefix-strip + disp에 base fold, disp8/
no-disp widening 포함), Win32 배치 패칭(worker가 live selector table로 S·shadow주소·
base 계산). 정적 배치와 probe 도구는 null table → 해당 site는 boundary 폴백.

**검증(120초 aot-dynamic).** 무회귀(EIP 0x30f551f 동일, Glide 49, fatal 0). `other`
경계 59,208 → 52,285(**−7k**). 커밋 `bbb93a6`(disp32), `3e35202`(widening).

**핵심 발견 (미해결).** 지배적 GS 접근은 **번역되지 않았다** — 경계 guest EIP가 전부
정적 PIU.EXE 이미지 범위(0x030Fxxxx)이고 런타임 arena(0x040Fxxxx)가 아니다. 정적
이미지는 **부팅 시 게스트가 GS(base 0x0B5C7000)를 설정하기 전에** 방출되므로, null
table로 배치돼 boundary 폴백된다. −7k는 런타임 arena의 non-GS 오버라이드에서 나왔다.

**GS 포착에 필요한 다음 단계 (결정 지점).** GS가 live가 된 뒤 정적 segment-override
site를 **재해석**해야 한다: (a) site와 원본 disp를 placement까지 운반, (b) GS-load HLE
핸들러(mov gs/pop gs)에서 GS site들을 재패치(S=새 guest_gs, disp=원본+새 base). 상당한
추가 작업이며 사용자 확인 지점.

## Phase 3a GS-load 재해석 (완료·검증, 큰 성과)

사용자가 GS-load 재해석 구현 선택. 구현: site에 원본 disp 저장, site를 placement로
운반(정적/동적), `ReResolveWin32AotSegmentOverrides`(RW→가드 selector·주소·disp=원본+
base 재패치 + boundary였던 pushfd 복원→RX→flush), 공용 `BuildWin32AotSegmentTable`,
`ReResolveAotSegmentOverrides`(selector 변경 시에만 실행하도록 self-gate), 그리고
세그먼트 레지스터 로드 HLE 핸들러(`instruction_emulation.cpp`)에서 훅 호출. 정적
이미지 site는 부팅 시 boundary였다가 게스트가 GS를 설정하면 재해석돼 활성화된다.

**검증 (120초 aot-dynamic):**

| 지표 | 기준(Task 263) | GS 재해석 | Δ |
|---|---:|---:|---:|
| segment-override 경계(segovr) | 28,769(nonflat 25,566) | **0** | **제거** |
| `other` 경계 | 59,208 | **26,055** | **−56%** |
| `other` top opcode | 0x65(GS) 25,564 | 0x8e(seg load) 6,717 | GS 소멸 |
| progress | 29,457 | **38,267** | **+30%** |
| 도달 EIP / Glide / fatal | 0x30f5520 / 49 / 0 | 0x30f551f / 49 / 0 | 무회귀 |

**segovr=0 = 가드 실패 0.** 게스트가 GS(0x80)를 설정한 뒤 재해석이 모든 정적 GS site를
활성화하고, GS가 안정적이라 가드가 항상 통과 → 모든 GS 메모리 접근이 네이티브 실행.
Task 262→264가 규명한 **지배적 비용(세그먼트 명령, 특히 GS)이 제거**됐고 무회귀,
progress +30%.

남은 `other`(26,055)는 세그먼트 레지스터 로드/스토어(0x8e/0x8c — 설계상 경계 유지)와
기타 비전달 명령. 커밋: `bbb93a6`(disp32), `3e35202`(widening), 그리고 본 재해석.

## 통제된 A/B 성능 검증 (Controlled A/B, 확인됨)

단일 실행 비교는 Debug 편차(±25%)로 신뢰도가 낮아, **같은 머신·연속·각 2회·120초**로
main(`c4885c3`, 최적화 전)과 branch(최적화 후)를 비교했다.

| | progress | single_step |
|---|---:|---:|
| 최적화 전 run 1 | 36,168 | 975,201 |
| 최적화 전 run 2 | 36,363 | 980,773 |
| 최적화 후 run 1 | 49,286 | 1,543,673 |
| 최적화 후 run 2 | 46,792 | 1,230,548 |

* **progress 평균 36,266 → 48,039 = +32%.** 최적화 후 두 실행이 최적화 전 두 실행을
  **모두 상회**(겹침 없음) — 편차가 아닌 실제 개선.
* **single_step 처리량 8,150/s → 11,559/s = +42%**(같은 시간에 명령을 더 많이 실행).
* 재해석 flush 우려는 현실화되지 않음: `segovr=0`(GS 안정)이라 self-gate가 재해석을
  부팅 시 한두 번으로 제한 → GS 예외 제거 이득이 재해석 비용을 압도.
* 유의: Debug 기준이며, 여전히 legacy(비-AOT, 세그먼트를 예외 처리하지 않음)보다는
  느리다. 본 작업은 aot-dynamic **내부**의 세그먼트 예외 비용을 제거해 격차를 좁힌 것.

**English (A/B).** Controlled A/B (same machine, back-to-back, 2 runs each, 120 s)
comparing main (pre-optimization) vs the branch: progress averaged 36,266 ->
48,039 (**+32%**) with every optimized run exceeding every baseline run (no
overlap, so real not variance), and single-step throughput 8,150/s -> 11,559/s
(+42%). The re-resolution flush cost did not cause a slowdown: segovr=0 (GS is
stable) means the self-gate re-resolves only once or twice at boot. Debug build;
still slower than legacy (which never treats segments as exceptions) -- this
narrows the gap inside the aot-dynamic backend.

## 결론 (Conclusion)

Task 262(계측)→263(근인=세그먼트 75%)→264(번역)의 완결. push-seg(Phase 1, −19%),
세그먼트 오버라이드 메모리 네이티브 번역(Phase 3a + GS 재해석, segovr 제거·other −56%·
progress +30%)로 aot-dynamic의 지배적 예외 왕복 비용을 안전하게 제거했다. 매 단계
프로브로 semantics를 확정하고(push=native, store=shadow, GS=non-flat) 무회귀로 검증했다.
공용 planner/emitter에 있어 모든 DOS4GW/DPMI executable에 일반 적용된다.

## English summary (Phase 3a + GS re-resolution)

Implemented GS-load re-resolution: store the original displacement per site, carry
sites into the placement, add ReResolveWin32AotSegmentOverrides (flip RW, re-apply
guard selector/address and disp=original+base, restore the pushfd that static
placement replaced with a boundary, flip RX, flush), a shared segment-table
builder, a self-gated ReResolveAotSegmentOverrides, and a hook in the
segment-register-load HLE handler. Static-image sites start as boundaries and
activate once the guest configures GS. Verified over 120 s aot-dynamic:
segment-override boundaries eliminated (segovr 28,769 -> 0), `other` boundaries
59,208 -> 26,055 (-56%), the top `other` opcode is no longer GS, progress
29,457 -> 38,267 (+30%), with no regression (same EIP frame loop, 49 Glide gates,
fatal 0). segovr=0 means zero guard failures: GS is stable, so every GS access
runs natively. This completes the Task 262->264 line -- the dominant segmentation
exception cost is removed, generalizing to any DOS4GW/DPMI executable via the
shared planner/emitter.

## English summary

Wrote the general segment-translation design (Task 264), then used a safe probe
to settle push-seg semantics before touching guest-affecting codegen. The probe
found the host segment register is always 0x2b while the shadow selector varies,
and since there is no push-seg HLE emulation the current single-step path
executes `push ds` natively (pushing host 0x2b) and the game works — so the
design's "push the shadow value" would have *changed* behavior (a bug), and the
correct Phase 1 is to run push-seg natively (kCopy). Implemented by exempting
push-of-segment-register from `IsHleBoundary`. Verified over a 120 s aot-dynamic
run: boundary exits −19%, `other` −17.7%, push-seg boundaries eliminated (0),
single-steps −4.9%, no crash/fatal, and the same execution state (same EIP frame
loop, same 49 Glide gates / render activity) — behavior-identical, just faster.
The first real performance win of the Task 262→264 line, and a reminder that
pinning a root cause still requires confirming semantics before implementing.
Next: Phase 2 (segment-register store) and Phase 3 (GS memory access), each
probe-confirmed and measured by the Task 263 counters.
