# Task 728 작업 로그 — LFB staging shadow 재사용

설계: [20260922-728](../design/20260922-728-linux-x64-lfb-staging-shadow-reuse.md)
작업 지시: [20260922-728](../work-orders/20260922-728-linux-x64-lfb-staging-shadow-reuse.md)

## 결과

`grLfbLock`이 write lock마다 수행하던 full framebuffer readback과 565 encode를, staging
surface가 이미 그 framebuffer를 담고 있다고 host가 증명할 수 있을 때 건너뛸 수 있게 했습니다.
근거는 `grLfbUnlock`이 staging surface **전체**를 present한다는 기존 사실입니다. 성공한 unlock
직후에는 surface가 곧 framebuffer이므로, 그 사이에 framebuffer를 바꿀 수 있는 gate가 없었다면
다음 lock은 GPU에 되물을 필요가 없습니다.

새 `GlideLfbStagingShadowState`는 그 사실을 buffer, color format, width, height와 함께 들고
있습니다. 무효화는 `GlideOrdinalPreservesLfbStagingShadow`라는 **기본값이 무효화인 allowlist**로
구현했으므로, 분류되지 않은 gate와 이후 추가될 gate는 자동으로 기존 seed 경로로 돌아갑니다.
flip된 present, 실패한 present, buffer·format·해상도 불일치도 모두 무효화입니다. 무효화 계수는
valid→invalid 전이에서만 올라가므로, 이미 무효인 상태에서 지나가는 gate가 수치를 채우지 않습니다.

Task 476의 region shadow가 같은 surface를 region gate 연속 호출에 대해 다루는 것과 달리, 이
shadow는 두 lock 사이의 state setter를 견디도록 설계했습니다. 그래서 술어가 gate마다 무조건
무효화하지 않고 분류를 참조합니다.

`REPIU_GLIDE_LFB_STAGING_SHADOW_CENSUS=1|on|true`는 동작을 바꾸지 않고 재사용 가능했던 lock을
계수하고, `REPIU_GLIDE_LFB_STAGING_REUSE=1|on|true`는 실제로 seed를 건너뜁니다. 후자는 전자를
함의하며 둘 다 기본 OFF입니다. 기본 실행에서는 상태 갱신도 계수도 하지 않습니다.

## 함께 확인된 사실 — readback 왕복은 무손실이 아니다

작업 중 `ReadbackFramebuffer`가 논리 framebuffer의 1:1 복사가 아님을 코드에서 확인했습니다.
window drawable 전체를 읽어 nearest-neighbor로 논리 해상도로 축소하고, `PresentLfbSurface`는
다시 확대합니다. drawable이 논리 해상도의 정수배가 아니면 present → readback 왕복이 원래
pixel을 복원하지 못합니다. 따라서 shadow 재사용은 정확도를 희생하는 최적화가 아니라 그 왕복
resample을 제거합니다. 이 사실은 `docs/analysis/linux-port-frontier.md`와 `ARCHITECTURE.md`에
기록했습니다. 게임 관찰이 아니라 코드 판독으로 확인한 사실입니다.

## 검증

- Win32 x86 Debug 전체 빌드 성공.
- `repiu_aot_probe --glide-lfb-staging-shadow` 9개 그룹 전부 통과: policy, 무효화 gate 분류,
  보존 gate 분류, reuse 시퀀스, census-only 시퀀스, gate 무효화, buffer·format·해상도 불일치,
  present 결과, 사유 이름.
- 인접 probe `--glide-lfb-timing`, `--glide-lfb-write-footprint`,
  `--glide-lfb-native-store-census` 통과.
- Win32 x86 `repiu_core_probe`: 28개 중 실패 0. Tasks 725~727이 기록한
  `mode16_push_writes=false` 실패는 이 빌드에서 재현되지 않았습니다. 이 작업은 그 그룹을 고치지
  않았으므로 해결의 증거가 아니라 재현되지 않았다는 기록입니다.

## 수행하지 못한 것

Linux VM(`192.168.198.132`)에 연결할 수 없어(연결 시간 초과) **Linux x64 빌드와 게임 관찰을
수행하지 못했습니다.** 그러므로 다음 두 가지는 아직 증거가 없습니다.

1. 게임에서 두 lock 사이에 무효화 gate가 얼마나 자주 끼어드는지, 즉 실제 재사용 가능 비율.
   프레임마다 `grBufferSwap`이 들어가는 구간에서는 재사용이 거의 일어나지 않을 수 있습니다.
2. apply 모드에서 화면이 이전과 동일한지.

VM 접근이 회복되면 census 모드로 비율을 먼저 측정하고, 그 수치를 본 뒤에만 apply 모드를
평가해야 합니다. 관찰은 `REPIU_EXECUTION_TIMEOUT_MS` 예산 만료 또는 SIGTERM으로 끝내야 final
report가 남습니다(`docs/guides/linux-shutdown-check.md`). Task 727의 top-EIP 관찰도 같은 이유로
아직 미완입니다.

---

# English

## Task 728 work log — LFB staging shadow reuse

Design: [20260922-728](../design/20260922-728-linux-x64-lfb-staging-shadow-reuse.md)
Work order: [20260922-728](../work-orders/20260922-728-linux-x64-lfb-staging-shadow-reuse.md)

## Result

The full framebuffer readback and 565 encode that `grLfbLock` ran for every write lock can now
be skipped when the host can prove the staging surface already holds that framebuffer. The
basis is an existing fact: `grLfbUnlock` presents the **entire** staging surface. Right after a
successful unlock the surface is the framebuffer, so unless a gate that can change the
framebuffer intervened, the next lock need not ask the GPU for it back.

The new `GlideLfbStagingShadowState` holds that fact together with buffer, color format, width
and height. Invalidation is `GlideOrdinalPreservesLfbStagingShadow`, an **allowlist whose
default is to invalidate**, so unclassified gates and gates added later fall back to the
existing seed path automatically. A flipped present, a failed present, and any buffer, format or
resolution mismatch invalidate as well. Invalidation counters advance only on the valid-to-
invalid transition, so gates passing over an already-invalid shadow do not inflate them.

Unlike Task 476's region shadow, which covers the same surface across a burst of region gates,
this shadow is built to survive the state setters between two locks, which is why the predicate
consults a classification instead of invalidating unconditionally at every gate.

`REPIU_GLIDE_LFB_STAGING_SHADOW_CENSUS=1|on|true` counts reusable locks without changing
behavior; `REPIU_GLIDE_LFB_STAGING_REUSE=1|on|true` actually skips the seed. The latter implies
the former and both are off by default; a default run neither updates the state nor counts.

## A fact confirmed along the way — the readback round trip is not lossless

While reading the code, `ReadbackFramebuffer` proved not to be a 1:1 copy of the logical
framebuffer. It reads the whole window drawable and nearest-neighbor downsamples to the logical
resolution, and `PresentLfbSurface` upscales again. When the drawable is not an integer multiple
of the logical resolution, the present-then-readback round trip does not restore the original
pixel. Reusing the shadow therefore removes that resampling round trip rather than trading
accuracy for speed. The fact is recorded in `docs/analysis/linux-port-frontier.md` and
`ARCHITECTURE.md`. It was established by reading the code, not measured from a run.

## Verification

- Full Win32 x86 Debug build succeeded.
- All nine groups of `repiu_aot_probe --glide-lfb-staging-shadow` passed: policy, invalidating-
  gate classification, preserving-gate classification, the reuse sequence, the census-only
  sequence, gate invalidation, buffer/format/resolution mismatch, present outcomes, and reason
  names.
- The neighboring `--glide-lfb-timing`, `--glide-lfb-write-footprint` and
  `--glide-lfb-native-store-census` probes passed.
- Win32 x86 `repiu_core_probe`: zero failures of 28. The `mode16_push_writes=false` failure
  recorded by Tasks 725 through 727 did not reproduce in this build. This task did not change
  that group, so this records a non-reproduction rather than evidence of a fix.

## What was not done

The Linux VM (`192.168.198.132`) could not be reached -- the connection timed out -- so the
**Linux x64 build and in-game observation were not performed.** Two things therefore have no
evidence yet.

1. How often an invalidating gate falls between two locks in the game, that is, the real
   reusable fraction. In stretches where a `grBufferSwap` lands every frame, reuse may almost
   never fire.
2. Whether the screen in apply mode is identical to before.

Once VM access returns, measure the fraction in census mode first and evaluate apply mode only
after seeing it. Observations must be ended through `REPIU_EXECUTION_TIMEOUT_MS` budget expiry
or SIGTERM for the final report to survive (`docs/guides/linux-shutdown-check.md`). Task 727's
top-EIP observation is still outstanding for the same reason.
