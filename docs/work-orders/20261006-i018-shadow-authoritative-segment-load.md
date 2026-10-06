# 작업 지시: shadow 진실원 세그먼트 로드 슬롯 (issue #18, 방향 3)

설계: `docs/design/20261006-i018-shadow-authoritative-segment-load.md`

## 작업 항목

1. shadow selector 블록 확장 (`src/runtime/aot_shadow_selector_block.*`)
   * `accepted_pair[6][2]` 워드 추가. 생성 시 `pair[reg][0]`에 호스트
     flat selector, `pair[reg][1]`은 0. 블록 생성 시 selector들을
     `context->guest_*`로 씨딩.
2. i386 guarded load 슬롯 재설계 (`src/runtime/aot_code_cache.cpp`
   `EmitGuardedSegmentLoadSlot`)
   * 물리 비교 제거, `[shadow]`/`[pair0]`/`[pair1]` 비교와
     `[shadow] ← 새 값` 기록으로 교체.
   * `AotGuardedSegmentLoadSite`에 `pair0_address_offset`,
     `pair1_address_offset`, `shadow_store_offset` 추가.
3. 패처 (`src/runtime/aot_segment_patch.cpp`
   `PatchAotGuardedSegmentLoadSites`)
   * 새 필드가 있으면 쌍 주소·shadow 기록 주소를 패치. 0이면(long-mode
     슬롯) 기존 동작.
4. 엔진
   * HLE 로드(`RecordGuestSegmentLoad`): base 0 fold·descriptor 있는
     non-flat selector 수용 시 `pair[reg][1]` 갱신.
   * DPMI set-base(AX=0007): 해당 selector가 쌍에 있으면 0으로 무효화.
   * `BuildAotSegmentTable`: shadow가 있으면 selector를 shadow에서
     읽고 `guest_*`를 맞춤(역전).
   * `SyncGuestSegmentsFromShadow` 헬퍼, `HandleDosInterrupt21`·
     `HandleDpmiInterrupt31`·glide boundary 진입부에 호출.
5. probe
   * `selector_guard_probe`의 로드 슬롯 레이아웃 단언을 새 슬롯으로
     갱신, 쌍 매치·쌍 밖 폴백 동작 단언 추가.
6. 검증 (설계 문서의 절차)
   * probe 통과, pumpitea 통제 프로토콜 측정, 쌍 밖 selector의 HLE
     경유 확인, 타 게임 기동 스모크.

## 완료 기준

* pumpitea 로딩 중 memcpy flip이 INT3 없이 돌고(해당 지점의 segment
  trace 소멸), guarded load 폴백·ES 재해석·재패치 횟수가 flip에
  비례하지 않는다.
* 공백이 통제 프로토콜에서 기준선보다 짧거나 같다(역설 재발 시 측정
  결과와 함께 기각 기록).

---

# Work order: shadow-authoritative segment-load slot (issue #18, direction 3)

Design: `docs/design/20261006-i018-shadow-authoritative-segment-load.md`

Items: (1) extend the shadow selector block with `accepted_pair[6][2]`
(flat selector in `pair[reg][0]` at creation, selectors seeded from
`context->guest_*`); (2) re-emit the i386 guarded load slot without the
physical compare, comparing `[shadow]`/`[pair0]`/`[pair1]` and storing
`[shadow] ← new`, with three new site offsets; (3) patch the new
operands, keeping the old behavior when the fields are 0 (long-mode);
(4) engine: update `pair[reg][1]` on HLE acceptance of base-0-folding
non-flat selectors, invalidate it on DPMI set-base, invert
`BuildAotSegmentTable` to read selectors from the shadow, and add
`SyncGuestSegmentsFromShadow` at the named HLE entries; (5) update the
selector-guard probe's load-slot assertions and add pair-match and
out-of-pair fallback assertions; (6) verify per the design.

Done when the memcpy flip runs without INT3 during pumpitea loading,
the fallback/re-resolve/re-patch counts no longer scale with flips, and
the gap is no worse than baseline under the controlled protocol.
