# Task 437 작업 지시 — 텍스처 상태 setter 생략을 A/B 가능 상태로

설계: [20260807-437](../design/20260807-437-glide-texture-setter-elision.md)

## 1. 범위

`grTexClampMode`·`grTexFilterMode`·`grTexMipMapMode`를 **opt-in 스위치 뒤에서** Task 365
생략 대상에 추가합니다. **기본 동작은 바꾸지 않습니다** — 승격은 사용자 A/B 뒤 별도
작업입니다.

**건드리지 않을 것:** `grTexSource`(포인터 인자), `grDepthMask`,
`grConstantColorValue`, combine 2종, `grDitherMode`, 기존 batch 1 목록, 생략 기구
자체(키·세대·무효화 규칙).

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `include/repiu/platform/win32/glide_setter_state_cache.h` | `GlideSetterTextureStateElisionEnabled()`, `IsGlideSetterTextureStateElisionGate()`, 스냅샷에 `texture_state` 필드 |
| `src/platform/win32/telemetry/glide_setter_state_cache.cpp` | opt-in 해석(`runtime::ResolveOptInToggle`), 텍스처 3종 목록, 스냅샷 반영 |
| `src/platform/win32/boundary/linexe_glide_boundary.cpp` | `elision_candidate_` 판정에 텍스처 목록을 조건부로 더함 |
| `src/host/win32/main.cpp` | 생략 요약 로그에 `texture-state` 표기 |
| `src/tools/aot_probe/glide_setter_state_cache_probe.cpp` | 스위치별 멤버십 단정 추가 |
| `docs/guides/glide-setter-elision-testing.md` | batch 2 A/B 절차 |
| `README.md` | 새 환경 변수 항목 |

## 3. 구현 규칙

* **기본값을 바꾸지 않습니다.** 미설정은 지금과 동일한 batch 1입니다.
* **기존 kill switch가 상위입니다.** `REPIU_GLIDE_SETTER_ELIDE=0`이면 텍스처 스위치도
  무효여야 합니다.
* **`grTexSource`는 양쪽 모두에서 제외**이며, probe가 그 사실을 단정으로 고정합니다.
* 생략은 host rendezvous만 건너뜁니다. 게이트 진입·ABI 검증·stdcall 정리·반환·호출
  순서는 그대로입니다(365의 계약).

## 4. 검증

1. Win32 Debug 빌드 통과.
2. `repiu_aot_probe.exe`가 `glide_setter_state_cache=true`로 통과 — 스위치 OFF/ON
   멤버십, `grTexSource` 제외, 텍스처 세대 무효화.
3. 짧은 스모크에서 Glide 구현 공백 0/0/0/0/0/0 유지.
4. (사용자) 같은 구간 3회 이상 A/B 캡처 — `same` 비율, ordinal 단가, 프레임 수.

## 5. 완료 기준

1. 미설정 실행이 이전과 동일하게 동작합니다.
2. `REPIU_GLIDE_SETTER_ELIDE_TEXTURE=1`에서 생략 요약의 `elided`가 텍스처 3종만큼
   늘고 화면 회귀가 없습니다.
3. 가이드만 보고 A/B를 재현할 수 있습니다.

---

# Task 437 Work Order — make the texture-state elision A/B-able

## 1. Scope

Add `grTexClampMode`, `grTexFilterMode` and `grTexMipMapMode` to the Task 365 elision set
**behind an opt-in switch**. **The default behaviour does not change**; promotion is a separate
task after the user's A/B. Not touched: `grTexSource` (pointer argument), `grDepthMask`,
`grConstantColorValue`, the combine setters, `grDitherMode`, the batch-one list, and the elision
machinery itself (key, generation and invalidation rules).

## 2. Files

The cache header and source gain the opt-in policy, the three-gate list and a `texture_state`
snapshot field; the boundary adds the conditional list to its elision candidacy test; the loader
prints the flag in the elision summary; the cache probe gains membership assertions for both
switch positions; and the elision testing guide and README document the batch-two procedure and
the new variable.

## 3. Implementation rules

The default stays batch one. The existing `REPIU_GLIDE_SETTER_ELIDE=0` kill switch must still win
over the new switch. `grTexSource` is excluded in both positions and the probe pins that.
Elision continues to skip only the host rendezvous — gate entry, ABI validation, stdcall cleanup,
return and call order are untouched, as Task 365 contracted.

## 4. Verification

The Debug build passes; `repiu_aot_probe.exe` reports `glide_setter_state_cache=true` with the
new membership and generation assertions; a short smoke keeps the Glide implementation-gap
counters at zero; and the user's paired capture supplies the `same` ratio, the per-ordinal unit
cost and the frame effect.

## 5. Done when

An unset run behaves exactly as before, `REPIU_GLIDE_SETTER_ELIDE_TEXTURE=1` raises `elided` by
the three gates' calls with no visual regression, and the guide alone is enough to reproduce the
A/B.
