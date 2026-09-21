# Task 728 작업 지시: LFB staging shadow 재사용

설계: [20260922-728](../design/20260922-728-linux-x64-lfb-staging-shadow-reuse.md)

## 한국어

### 범위

`grLfbLock`의 무조건적인 framebuffer readback과 565 encode를, staging surface가 이미
framebuffer를 정확히 담고 있다고 host가 증명할 수 있을 때 건너뛸 수 있게 합니다. 기본
동작은 바꾸지 않습니다.

### 구현 항목

1. `include/repiu/engine/glide_lfb_staging_shadow.h` 신설
   * `GlideLfbStagingShadowInvalidationReason` 열거: `kGate`, `kLockHandoff`,
     `kPresentFailed`, `kFlippedPresent`, `kSurfaceMismatch`, `kShutdown`.
   * `GlideLfbStagingShadowState`: `valid`, `buffer`, `color_format`, `width`, `height`와
     counter(`lock_count`, `reusable_lock_count`, `reused_lock_count`, `seed_count`,
     `validate_count`, 사유별 `invalidate_counts`).
   * `GlideLfbStagingShadowSnapshot`와 `SnapshotGlideLfbStagingShadow`.
   * 정책 함수: `ResolveGlideLfbStagingShadowSetting`,
     `GlideLfbStagingShadowCensusEnabled`, `GlideLfbStagingReuseEnabled`.
   * 술어 `GlideOrdinalPreservesLfbStagingShadow(repiu::hle::GlideGateId)`.
   * 상태 전이: `ValidateGlideLfbStagingShadow`, `InvalidateGlideLfbStagingShadow`,
     `CanReuseGlideLfbStagingShadow`, `NoteGlideLfbStagingShadowLock`.
2. `src/engine/glide_lfb_staging_shadow.cpp` 신설. 동적 할당과 정렬을 쓰지 않습니다.
3. `src/engine/execution/thread_context.h`에 `glide_lfb_staging_shadow` 상태를 추가합니다.
4. `src/engine/boundary/linexe_glide_boundary.cpp` 통합
   * gate dispatch 직전의 기존 region-shadow 무효화 지점에서 술어에 따라 staging shadow도
     무효화합니다.
   * `kGrLfbLock` write 경로: seed 직전에 재사용 가능 여부를 계수하고, apply 모드면 readback과
     encode를 건너뜁니다. 어느 쪽이든 직후에 `kLockHandoff`로 무효화합니다.
   * `kGrLfbUnlock` write 경로: `PresentLfbSurface` 성공이고 `flip_v`가 false일 때만
     validate하고, 그 밖에는 사유를 붙여 무효화합니다.
5. `include/repiu/engine/execution_trampoline.h`에 snapshot 필드를 추가하고
   `src/engine/telemetry/live_telemetry_snapshot.cpp`에서 채웁니다.
6. `src/host/win32/main.cpp` 최종 보고에 census 줄을 추가합니다.
7. `src/tools/aot_probe/glide_lfb_staging_shadow_probe.{h,cpp}`와 `--glide-lfb-staging-shadow`
   등록, `CMakeLists.txt` 갱신.
8. 문서: `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md`(readback 왕복 손실 사실 포함),
   작업 로그.

### 검증

* Win32 x86 Debug 전체 빌드.
* `repiu_aot_probe --glide-lfb-staging-shadow` 통과.
* Win32 x86 `repiu_core_probe` 실행 후 기존 실패와 비교.
* Linux x64 빌드와 게임 관찰은 VM 접근 회복 뒤로 미루고, 작업 로그에 미완료로 남깁니다.

### 하지 않을 것

* 어떤 기본값도 켜지 않습니다.
* `ReadbackFramebuffer`의 resample 동작 자체는 이 작업에서 고치지 않습니다. 사실만 기록합니다.
* `grBufferSwap` 경로는 건드리지 않습니다.

---

## English

### Scope

Allow the unconditional framebuffer readback and 565 encode in `grLfbLock` to be skipped when
the host can prove the staging surface already holds the framebuffer exactly. Default behavior
is unchanged.

### Implementation items

1. New `include/repiu/engine/glide_lfb_staging_shadow.h`
   * `GlideLfbStagingShadowInvalidationReason`: `kGate`, `kLockHandoff`, `kPresentFailed`,
     `kFlippedPresent`, `kSurfaceMismatch`, `kShutdown`.
   * `GlideLfbStagingShadowState`: `valid`, `buffer`, `color_format`, `width`, `height` and
     counters (`lock_count`, `reusable_lock_count`, `reused_lock_count`, `seed_count`,
     `validate_count`, per-reason `invalidate_counts`).
   * `GlideLfbStagingShadowSnapshot` and `SnapshotGlideLfbStagingShadow`.
   * Policy: `ResolveGlideLfbStagingShadowSetting`, `GlideLfbStagingShadowCensusEnabled`,
     `GlideLfbStagingReuseEnabled`.
   * Predicate `GlideOrdinalPreservesLfbStagingShadow(repiu::hle::GlideGateId)`.
   * Transitions: `ValidateGlideLfbStagingShadow`, `InvalidateGlideLfbStagingShadow`,
     `CanReuseGlideLfbStagingShadow`, `NoteGlideLfbStagingShadowLock`.
2. New `src/engine/glide_lfb_staging_shadow.cpp`, with no dynamic allocation or sorting.
3. Add the `glide_lfb_staging_shadow` state to `src/engine/execution/thread_context.h`.
4. Integrate in `src/engine/boundary/linexe_glide_boundary.cpp`
   * Invalidate the staging shadow by the predicate at the existing region-shadow invalidation
     point just before gate dispatch.
   * `kGrLfbLock` write path: count reusability right before the seed and, in apply mode, skip
     readback and encoding. Either way, invalidate with `kLockHandoff` immediately afterwards.
   * `kGrLfbUnlock` write path: validate only when `PresentLfbSurface` succeeded and `flip_v` is
     false; otherwise invalidate with the matching reason.
5. Add the snapshot field to `include/repiu/engine/execution_trampoline.h` and fill it in
   `src/engine/telemetry/live_telemetry_snapshot.cpp`.
6. Add census lines to the final report in `src/host/win32/main.cpp`.
7. Add `src/tools/aot_probe/glide_lfb_staging_shadow_probe.{h,cpp}`, register
   `--glide-lfb-staging-shadow`, and update `CMakeLists.txt`.
8. Documentation: `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md` (including the
   lossy readback round-trip fact), and the work log.

### Verification

* Full Win32 x86 Debug build.
* `repiu_aot_probe --glide-lfb-staging-shadow` passes.
* Run the Win32 x86 `repiu_core_probe` and compare against the known existing failure.
* Defer the Linux x64 build and in-game observation until VM access returns, and record them as
  outstanding in the work log.

### Not to be done

* Do not turn on any default.
* Do not change the resampling behavior of `ReadbackFramebuffer` itself; only record the fact.
* Do not touch the `grBufferSwap` path.
