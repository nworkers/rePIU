# Task 445 작업 지시 — 인라인 캐시 패치를 게스트 스레드에서 직접

설계: [20260807-445](../design/20260807-445-inline-cache-patch-on-guest-thread.md)

## 1. 범위

`RequestAotInlineCachePatch`가 워커 이벤트 왕복 대신 `PatchWin32AotIndirectInlineCache`를
**그 자리에서** 호출하게 합니다. `REPIU_AOT_INLINE_CACHE_PATCH_INLINE` opt-in,
**기본값은 꺼짐**.

**건드리지 않을 것:** 번역·retire 요청 경로(워커 유지), 패치 함수 자체, 인라인 캐시
엔트리 선택 규칙, W^X 규율(RW→쓰기→RX→flush 순서 그대로).

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `aot_runtime_dispatch.cpp/.h` | 정책 함수와 직접 호출 분기, 직접·워커 경로별 카운터 |
| `main.cpp` | 요약에 인라인 패치 경로와 횟수 |
| `inline_cache_probe.cpp` | 정책 해석 단정 |
| `README.md` | 새 변수 |

## 3. 구현 규칙

* **결과 의미를 바꾸지 않습니다.** `context->aot_inline_cache_patch_result`와 반환값은
  두 경로에서 동일해야 합니다.
* **W^X 순서를 그대로 둡니다.** 보호 전환·쓰기·복원·flush는 패치 함수 안에 있고 그대로
  씁니다.
* 스위치가 꺼져 있으면 지금과 **완전히 같은 경로**입니다.
* 직접 경로에서도 **워커 타이밍 계정을 잃지 않도록** 별도 카운터를 남깁니다 — 그래야
  A/B에서 "왕복이 사라졌다"를 로그만으로 확인할 수 있습니다.

## 4. 검증

1. Release 빌드·probe 통과.
2. pumpit2 A/B — `aot worker timing other`가 1.7M → 거의 0, position census에서
   `RequestAotInlineCachePatch` 표본 소멸, 구현 공백 0, 크래시 0.
3. 프레임 수는 장면 편차가 크므로 **cycle 비중과 워커 카운터**로 판정합니다.

## 5. 완료 기준

`=0`이 지금과 동일하고, `=1`에서 워커 `other` 연산이 사라지며 게스트 표본이 캐시 실행
쪽으로 이동합니다.

---

# Task 445 Work Order — patch the inline cache on the guest thread

Behind `REPIU_AOT_INLINE_CACHE_PATCH_INLINE`, off by default, `RequestAotInlineCachePatch` calls
`PatchWin32AotIndirectInlineCache` in place instead of handing it to the worker. Translation and
retire keep the worker, the patch function itself is unchanged, and the W^X sequence inside it
stays exactly as it is.

The dispatch source gains the policy and the direct branch with per-path counters, the loader
prints them, the probe asserts the policy, and the README documents the variable. **The result
semantics must not change** between paths, and with the switch off the path must be today's.
Counters matter: without them the A/B cannot show from the log alone that the round trip is gone.

Verification is the Release build and probe, then a pumpit2 A/B showing `aot worker timing other`
falling from 1.7M to near zero and `RequestAotInlineCachePatch` vanishing from the position
census, with zero implementation gaps and no crash. Frames are not the criterion — scene variance
is too large — the worker counters and cycle shares are.
