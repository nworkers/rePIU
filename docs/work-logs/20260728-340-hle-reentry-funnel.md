# 20260728-340 작업 로그: HLE 복귀 funnel 귀속 / Work log: The HLE reentry funnel

## 한국어

### 결론 요약

**Task 339가 남긴 88.7%의 정체가 확정됐습니다. `IsGuestInstructionPointer`가 아니라
페이지 quarantine입니다.** 그리고 그 quarantine은 **단 4개**입니다.

| 거절 사유 | 횟수 | 비중 |
|---|---:|---:|
| **quarantined** | 150,341 | **80.24%** |
| segment-write | 15,471 | 8.26% |
| **success** | 21,561 | 11.51% |
| outside-arena | **0** | 0.00% |
| span-unsafe | 0 | 0.00% |
| cache miss | 0 | — |
| not-pending / backend | 0 / 0 | 0.00% |

(Release 60초 baseline, progress 127,069, 프레임 2,077, 정상 timeout)

**확인됨:** EIP가 arena 밖이어서 거절된 경우는 **0건**입니다. Task 339가 두 조건을
묶어 놓았던 탓에 남아 있던 가설이 이렇게 갈립니다.

**확인됨:** cache miss도 **0건**입니다. 즉 quarantine과 segment-write만 통과하면
대상은 **항상** 캐시에 있습니다. post-HLE 번역 분기가 도달 불가라는 Task 339의 결론이
재확인됩니다.

### 4개의 페이지가 80%를 막습니다

같은 실행의 요약은 `Win32 AOT generation publishes/quarantines: 145/4`입니다.
**quarantine된 페이지는 4개뿐인데 post-HLE 복귀 시도의 80.24%가 그 위에서
거절됩니다.**

quarantine이 걸리는 경로는 둘입니다.

1. guest가 **자기 페이지에 코드를 쓰는 경우**(`same_page`) — 자기수정 코드 보호.
2. retired target에 대한 동적 번역 실패(이 실행에서 generation failure 1회).

즉 이 게임의 가장 뜨거운 코드 몇 페이지가 자기수정(또는 코드 인접 데이터 쓰기)으로
격리되었고, **그 위에서는 영구히 TF walk만 남습니다.** Task 337이 발견한 5~8개 구간과
33+ 꼬리가 여기서 나옵니다.

### `SUPERBLOCK=1` 실행은 이번에 조기 종료했습니다

`original entry raised a caught exception`으로 약 8,141 progress에서 끝났고 예외
총계도 58,753뿐이라 funnel 비교 대상이 아닙니다. Task 333이 기록한 간헐적 조기 crash와
같은 증상입니다. **다만 이번에는 `SUPERBLOCK=1`에서 나왔습니다.** 재현 조건은
여전히 미확정입니다.

### 그래서 다음이 좁혀집니다

* 대상은 **4개 페이지**입니다. 전면적 설계 변경이 아닙니다.
* 처방 후보 두 가지:
  1. 페이지를 격리하는 대신, **쓰기 대상 범위만 excluded range로 빼고 재번역**한다.
     그 기계장치(`AotExcludedGuestRange`)는 이미 있고 coherence probe가 검증한다.
  2. quarantine 페이지라도 **쓰기 이후 바이트가 변하지 않았음을 확인하면** 복귀를
     허용한다(native-span 음성 캐시가 쓰는 byte-validated 방식).
* 어느 쪽이든 **정확성 계약이 먼저**입니다. quarantine은 자기수정 코드로부터
  캐시를 보호하려고 존재합니다.

### 검증 결과

1. Release 전체 빌드 통과.
2. baseline 60초 실행 정상 timeout, malformed 0, fatal 0, Glide 공백 0,
   gate 진입 88,850, `grBufferSwap` 2,077 — Task 338이 추가한 동등성 축 통과.
3. funnel 합계 187,373 = 거절 + 성공 (구성상 배타적).

### 확인됨 / Confirmed

* post-HLE 복귀 거절의 80.24%가 **quarantine**, 8.26%가 segment-write이며
  `IsGuestInstructionPointer` 거절은 0건입니다.
* quarantine된 페이지는 **4개**입니다.
* 복귀가 lookup까지 가면 캐시 적중률 100%입니다.

### 미확정 / Unresolved

* 그 4개 페이지가 어디이고 왜 자기수정으로 분류됐는지.
* segment-write 8.26%는 별개 제약이며 이번 범위가 아닙니다.
* `SUPERBLOCK=1`의 조기 crash 재현 조건.

---

## English

### Summary

Task 339's unsplit 88.7% resolves cleanly: the rejection is page quarantine, not
`IsGuestInstructionPointer`, which rejected nothing at all. Of 187,373 post-HLE return attempts in
a 60-second Release baseline run, 150,341 (80.24%) were rejected as quarantined, 15,471 (8.26%) by
the segment-write probe, and 21,561 (11.51%) succeeded, with zero rejected for being outside the
arena, zero for span safety, and zero cache misses — so anything that clears quarantine and the
segment-write probe is always already in the cache, re-confirming that the post-HLE translation
branch is unreachable.

### Four pages block 80%

The same run reports `AOT generation publishes/quarantines: 145/4`. Four quarantined pages account
for 80.24% of all rejected returns. Quarantine is applied when the guest writes code on the page it
is executing from, which is self-modifying-code protection, or when dynamic translation of a
retired target fails (one generation failure in this run). So a few of this game's hottest pages
are quarantined and everything on them is walked under TF forever, which is where Task 337's
five-to-eight-step mode and long tail come from.

### The SUPERBLOCK run ended early

It stopped with `original entry raised a caught exception` at progress 8,141 with only 58,753
exceptions, so its funnel is not comparable. This is the same intermittent early crash Task 333
recorded, this time under `SUPERBLOCK=1`; the reproduction condition is still unknown.

### What this narrows

The target is four pages, not an architecture change. Two candidate remedies: re-translate the page
with only the written range excluded, using the existing `AotExcludedGuestRange` mechanism the
coherence probe already validates; or allow return into a quarantined page once the bytes are shown
unchanged, the byte-validated approach the native-span negative cache already uses. Either way the
correctness contract comes first, since quarantine exists to protect the cache from self-modifying
code.

### Verification and unresolved

The Release build passed and the baseline run reached its 60-second timeout with zero malformed
dispatch, no fatal halt, no Glide gap, 88,850 gate entries, and 2,077 buffer swaps, satisfying the
equivalence axes Task 338 added; the funnel total of 187,373 is exclusive by construction.
Unresolved: which four pages these are and why they are classified self-modifying, the separate
8.26% segment-write constraint, and the `SUPERBLOCK` early crash.
