# AOT 반환 인라인 캐시 2엔트리 확장 작업 지시서
# Work Order: Two-Way AOT Return Inline Cache

## 1. 목적 (Objective)

Task 219가 확정한 디코드 루프 ~1000배 감속(반환 대상 2개 교대로 인한 단일 엔트리 인라인 캐시
스래싱)을 해소하기 위해 반환 thunk의 인라인 캐시를 2엔트리로 확장한다. 설계는
`docs/design/20260716-220-aot-return-inline-cache-two-way.md` 참조.

## 2. 세부 작업 (Tasks)

1. `include/repiu/runtime/aot_code_cache.h`: `AotIndirectInlineCacheSite`에 second-entry 필드
   5종 추가.
2. `src/runtime/aot_code_cache.cpp`: `EmitReturnInlineCacheSlot`을 2엔트리 레이아웃으로 확장.
3. `src/platform/win32/aot_code_cache_win32.cpp`: (a) `AppendWin32AotDynamicImage`의 오프셋
   재배치에 second-entry 필드 반영, (b) `PatchWin32AotIndirectInlineCache`에 stateless 2엔트리
   채움/교체 정책 구현.
4. Debug 재빌드 후 `aot-dynamic pumpit1` 40초 구동으로 디코드 구간 처리량 회복을 확인하고,
   기본 trap 백엔드 30초 회귀를 확인한다.
5. 결과를 `docs/analysis/current-execution-frontier.md`와 작업 로그에 반영한다.

## 3. 검증 범위 (Verification Scope)

방출 코드와 패치 프로토콜을 바꾸는 실행 의미론 변경이므로 두 백엔드 교차 검증을 수행한다:
(1) `aot-dynamic` 40초 — `0x030EE1DA` boundary 고정 소멸 + dispatch/progress 재개 여부,
(2) 기본 trap 백엔드 30초 — 기존 progress 수준(~64만) 유지 여부.
