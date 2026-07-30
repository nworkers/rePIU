# 작업 지시: swap interval 강제 / Work order: swap interval override

Task 371. 설계: [20260731-371](../design/20260731-371-glide-swap-interval-override.md)

## 한국어

### 목표

`REPIU_GLIDE_SWAP_INTERVAL`로 SDL swap interval을 강제할 수 있게 하고, 그 A/B로
이 실행이 디스플레이에 제한되는지 판정한다. 미설정 시 동작 변화가 없어야 한다.

### 단계

1. **정책 모듈 추가**
   * `include/repiu/platform/win32/glide_swap_interval_policy.h`
   * `src/platform/win32/glide_swap_interval_policy.cpp`
   * `ResolveGlideSwapIntervalOverride(setting, *interval)` — `-1`~`4`만 수용,
     후행 공백·비숫자 거부.
   * `TryReadGlideSwapIntervalOverride(*interval)`.
   * `Win32GlideSwapIntervalPolicySnapshot` — 요청 여부/요청값/적용 성공/실효값/
     실효값 유효 여부.
   * `CMakeLists.txt` 등록.

2. **backend 적용**
   * `OpenWindowed`의 `SDL_GL_MakeCurrent` 성공 직후 1회 `SDL_GL_SetSwapInterval`.
   * 직후 `SDL_GL_GetSwapInterval`로 실효값 되읽기.
   * 미설정이면 SDL 호출을 **하지 않는다**(현행 유지).

3. **요약 출력**
   * `Win32 Glide swap interval override requested/value/applied/effective`

4. **probe 추가**
   * `glide_swap_interval_policy_probe.{h,cpp}` + `main.cpp` + `CMakeLists.txt`.
   * 수용(`-1` `0` `1` `4`) / 거부(`` `5` `-2` `1 ` `x`) / `nullptr` 무해 /
     snapshot 필드 전달.

5. **빌드·검증**
   * Debug + Release, probe exit 0.
   * 같은 장면 70초 고정으로 `REPIU_GLIDE_SWAP_INTERVAL=1`과 `=0` A/B.
   * 프레임 수와 함께 `timer tick delivery`, `INT 8 chain HLE count`를 읽는다.

6. **문서**
   * 작업 로그, `docs/analysis/glide-gate-cost-attribution.md`,
     `current-execution-frontier.md` 갱신.

### 완료 조건

* 미설정 실행이 변경 전과 동일하게 동작.
* 설정 시 요약에 요청값과 되읽은 실효값이 남음.
* A/B 결과로 디스플레이 제한 여부 판정.
* probe 전 항목 통과, 양 구성 빌드 성공.

### 비범위

* 게스트의 `grBufferSwap` interval 인자를 자동 반영하는 것은 **하지 않는다**
  (동작 변경이므로 판정 후 별도 결정).

---

## English

Add `REPIU_GLIDE_SWAP_INTERVAL` so the SDL swap interval can be forced, then use an
A/B to decide whether this run is display-limited. A new
`glide_swap_interval_policy` module resolves `-1` through `4` and reports whether
the override was requested, whether it applied, and the effective value read back;
the backend applies it once right after `SDL_GL_MakeCurrent` and makes no SDL call
at all when the variable is unset, so the default path is unchanged. Add a summary
line and a probe covering acceptance, rejection, null safety, and snapshot
propagation. Verify with both build configurations, a passing probe, and a
fixed-duration A/B at intervals 1 and 0 read alongside the timer tick delivery
lines, since this is a rhythm game whose timing could move with the frame rate.
Honouring the guest's own interval argument stays out of scope until the
measurement decides.
