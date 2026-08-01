# 20260801-388 Glide Gate 직접 Dispatch 기본 활성화 작업 로그 / Work Log

## 한국어

### 캡처 판독

- `single-step-glide-direct-capture.log`는 49.187초 동안 `_GRBUFFERSWAP@4` 3,957회와 `Glide buffer swapped` 상태를 기록했습니다.
- direct patched/verified/resolved/relinked/entry/success/miss/terminal은 `172/172/129/492/734,293/734,292/0/0`입니다. 종료 스냅샷의 한 호출만 처리 중이었고 final exception은 없습니다.
- 직전 2.750초 캡처와 길이가 달라 절대 합계는 비교하지 않았습니다. 총 예외율은 `48,492 / 2.750s = 17,633/s`에서 `185,728 / 49.187s = 3,776/s`로 약 78.6% 감소했습니다. 동일 5초 A/B의 63.22% 감소 결과와 방향이 일치합니다.

### 구현

- `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH` 미설정 기본값을 ON으로 승격했습니다.
- `1|on|true`는 계속 활성화하며 `0|off|false`, 빈 문자열, 알 수 없는 값은 opt-out입니다.
- selector-guard probe에 미설정/활성/opt-out/알 수 없는 값 정책 검사를 추가했습니다.

### 검증

- Win32 x86 Release 전체 빌드: 성공. 기존 C4819/LNK4217 경고만 남았습니다.
- `repiu_aot_probe` 두 PIU 실행 파일 구성: 모두 exit 0.
- 신규 `glide_direct_dispatch_policy=true`, 기존 `glide_direct_dispatch_layout=true`, `selector_guard_all=true`, `coherence_all=true`.
- 1초 기본 smoke: enabled `true`, patched/verified/resolved/relinked/entry/success/miss/terminal `172/172/8/8/9/9/0/0`.
- 1초 `0` opt-out smoke: enabled `false`, direct 계수 모두 0, 기존 gate handler `5/5`.
- 두 smoke 모두 의도한 timeout 종료이며 final exception은 없습니다.

## English

### Capture review

- `single-step-glide-direct-capture.log` recorded 3,957 `_GRBUFFERSWAP@4` calls and `Glide buffer swapped` over 49.187 seconds.
- Direct patched/verified/resolved/relinked/entry/success/miss/terminal was `172/172/129/492/734,293/734,292/0/0`. Only one call was in flight at the shutdown snapshot and there was no final exception.
- Absolute totals were not compared because the preceding capture lasted only 2.750 seconds. Total exception rate fell from `48,492 / 2.750s = 17,633/s` to `185,728 / 49.187s = 3,776/s`, approximately 78.6%, consistent with the matched five-second A/B reduction of 63.22%.

### Implementation

- Promoted an unset `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH` to ON.
- `1|on|true` still enables the path; `0|off|false`, an empty string, and unknown values opt out.
- Added unset/enabled/opt-out/unknown policy checks to the selector-guard probe.

### Verification

- Full Win32 x86 Release build passed with only pre-existing C4819/LNK4217 warnings.
- `repiu_aot_probe` exited 0 for both PIU executable variants.
- New `glide_direct_dispatch_policy=true`; existing `glide_direct_dispatch_layout=true`, `selector_guard_all=true`, and `coherence_all=true`.
- One-second default smoke: enabled `true`, patched/verified/resolved/relinked/entry/success/miss/terminal `172/172/8/8/9/9/0/0`.
- One-second `0` opt-out smoke: enabled `false`, all direct counters zero, legacy gate handler `5/5`.
- Both smokes ended at the intended timeout without a final exception.
