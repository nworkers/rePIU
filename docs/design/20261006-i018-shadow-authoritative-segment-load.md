# 설계: shadow를 진실원으로 하는 세그먼트 로드 슬롯 (issue #18, 방향 3)

## 근거가 되는 측정 (분석: `docs/analysis/pumpitea-loading-segment-flip.md`)

1. Win32에서 물리 segment 레지스터는 게스트 selector를 절대 담지
   않는다. VEH 재개는 재개 컨텍스트의 `SegEs` 쓰기를 무시한다
   (`repiu_aot_probe --segment-restore`로 확정).
2. 따라서 guarded load 슬롯의 `물리 == 새 값` 비교는 flat(0x2B) 값
   로드에서만 통과할 수 있고, 게스트 selector(0x0024) 로드는
   구조적으로 항상 INT3 폴백한다.
3. pumpitea의 게스트 memcpy는 `push es; mov ax,ds; mov es,ax;
   rep movsd; mov es,[saved]`로 ES를 매번 0x0024↔0x002B로 바꾸고,
   양쪽 모두 guarded **load** 지점이다. 전수 트레이스로 memcpy 1회 =
   폴백 2 + ES 재해석 2 + 캐시 전량 재패치 2가 현 설계의 하한임을
   확인했다(25초에 각 4,420회).
4. 두 selector 모두 base 0으로 접힌다. 네이티브 문자열 연산은 물리
   flat 세그먼트로 돌므로, base 0 selector 간의 가상 전환은 메모리
   매핑을 바꾸지 않는다 — 오늘의 HLE도 같은 근거로 이 전환을
   수용하고 있다.
5. 재패치 경로를 싸게 만드는 세 변형(v1·v2·v3)이 모두 공백을
   악화시켰다. 트랩을 유지한 채 뒤처리만 줄이는 설계는 반복해서
   반증되었으므로, **트랩 자체를 없애는 것**이 남은 유일한 방향이다.

## 설계

게스트의 가상 세그먼트 상태의 진실원을 shadow selector 블록으로 두고,
i386 guarded **load** 슬롯이 수용 가능한 selector 전환을 INT3 없이
shadow에 직접 기록하게 한다.

```mermaid
flowchart TD
    A["mov Sreg, r/m16 (게스트)"] --> B{"새 값 == [shadow]?"}
    B -->|예: no-op 재로드| S[성공 카운터, 통과]
    B -->|아니오| C{"새 값 == [pair0] 또는 [pair1]?"}
    C -->|예| W["[shadow] ← 새 값 (네이티브)"] --> S
    C -->|아니오| F[INT3 폴백 → HLE 로드\n(검증·등록·재해석은 여기서만)]
```

### 1. 슬롯의 수용 쌍: 레지스터별 메모리 워드 2개

* shadow selector 블록(4 KiB 전용 페이지)에 레지스터별
  `accepted_pair[6][2]`를 더한다. 슬롯은 **즉치가 아니라 이 워드와
  비교**하므로, 쌍이 바뀌어도 캐시 재패치가 필요 없다 — 엔진이 워드를
  쓰면 끝이다.
* `pair[reg][0]`: 호스트 flat selector(0x002B). 블록을 만들 때 엔진이
  한 번 쓴다.
* `pair[reg][1]`: HLE 로드가 **base 0으로 접히고 descriptor가 있는
  non-flat selector**를 수용할 때마다 그 값으로 갱신한다(pumpitea의
  0x0024). 그 selector의 descriptor base가 DPMI로 바뀌면(AX=0007 등)
  엔진이 워드를 0으로 지워 네이티브 수용을 멈춘다. 0은 어떤 로드
  값과도 일치하지 않는다(selector 0 로드는 HLE로 가야 한다).
* 수용 기준이 "base 0 fold"인 이유: 네이티브 실행(문자열 연산,
  접힌 override)은 물리 flat 세그먼트를 쓰므로, base 0이 아닌
  selector를 트랩 없이 수용하면 틀린 메모리에 닿는다. base 0 쌍
  안에서의 전환은 오늘의 HLE 수용과 같은 의미론이다.

### 2. 새 i386 guarded load 슬롯

물리 비교를 버리고(근거 1) 다음으로 바꾼다. 연산 폭과 피연산자
형태(r16/[esp])는 기존 슬롯과 같다.

```
pushfd; push eax
(mov ax, <새 값>)          ; 기존과 동일 (gpr 또는 [esp])
66 3B 05 <shadow>          ; cmp ax, [shadow]
je  success                ; no-op 재로드
66 3B 05 <pair0>           ; cmp ax, [pair0]
je  write
66 3B 05 <pair1>           ; cmp ax, [pair1]
jne fallback
write:
66 A3 <shadow>             ; mov [shadow], ax
success:
FF 05 <success_counter>
pop eax; popfd; jmp fallthrough
fallback:
FF 05 <fallback_counter>
pop eax; popfd; INT3
```

`AotGuardedSegmentLoadSite`에 `pair0_address_offset`,
`pair1_address_offset`, `shadow_store_offset`를 더하고, 패처는 shadow
주소와 쌍 주소를 쓴다. long-mode(x64 cache 모델) 슬롯은 이번에는
그대로 둔다 — 같은 전환이 x64에도 이롭지만 별도 측정이 필요하고,
패처는 새 필드가 0이면 기존 동작을 유지한다.

### 3. 진실원 역전: guest_* ← shadow

슬롯이 shadow를 직접 쓰면 `context->guest_es`가 낡는다. 동기 방향을
뒤집는다.

* `BuildAotSegmentTable`: shadow 블록이 있으면 **selector를 shadow에서
  읽고**, `context->guest_*`를 그 값으로 맞춘다(기존에는 반대 방향).
  블록이 없으면 기존 동작.
* shadow 블록 생성 시 `context->guest_*`로 **씨딩**한다(생성 전
  상태를 승계).
* shadow를 읽기 전에 `guest_*`를 소비하는 HLE 진입부 —
  `HandleDosInterrupt21`, `HandleDpmiInterrupt31`,
  LINEXE glide boundary의 세그먼트 소비 지점 — 에
  `SyncGuestSegmentsFromShadow(context)` 호출을 더한다. 헬퍼는 블록이
  없으면 아무것도 하지 않는다.
* `RecordGuestSegmentLoad`는 지금처럼 양쪽(guest_*, shadow)을 쓴다.

### 4. 바꾸지 않는 것

* HLE 폴백 경로의 의미(검증, descriptor 등록, 재해석·재패치)는
  그대로다. 쌍에 없는 selector는 오늘과 똑같이 HLE로 간다.
* override·pop·read 슬롯과 long-mode 슬롯은 이번 작업에서 그대로다.
  pop(ISR의 `pop ds`)과 read는 후속 단계 후보로 남긴다(ISR의 DS
  왕복은 초당 ~167회로 memcpy flip보다 드물다).
* 전량 재패치의 형태(마스크 없음, capacity 보호)는 그대로다 —
  v1~v3의 반증을 존중한다. 이 설계는 재패치의 **호출 빈도**를 flip
  경로에서 제거할 뿐이다.

## 예상 효과와 위험

* 로딩 공백에서 flip의 폴백·재해석·재패치(25초에 각 ~8.8천 회)가
  사라진다. 남는 것은 DOS 시간 루프·lseek·포트 I/O의 VEH들이다.
* 위험 1: 재패치 빈도가 급감하는 것 자체가 v1~v3 역설의 미지의
  기전과 상호작용할 수 있다. 판정은 측정 프로토콜(아래)로만 한다.
* 위험 2: `guest_*` 동기 누락 지점. 역전(3)은 BuildAotSegmentTable에
  집중시키고 서비스 진입부 동기를 더하지만, 전수 보장은 아니다.
  검증 항목에 세그먼트 상태 일관성 검사를 둔다.

## 검증

1. `repiu_aot_probe --selector-guard` 갱신·통과(새 슬롯 레이아웃
   단언 포함), `--segment-restore` 통과 유지.
2. pumpitea 90초, 통제 프로토콜(식힌 기계, exe 사본, 워밍업 1회 폐기,
   기준선 교대 ≥2쌍): 공백, handled segment load, guarded
   success/fallback, 재패치 카운터를 함께 기록한다.
3. 전환 수용이 보수적인지: `REPIU_DPMI_SEGMENT_TRACE`로 쌍 밖
   selector(0x002C, 0x0090 등)가 여전히 HLE로 가는지 확인한다.
4. 회귀 울타리: pumpit1·pumpit2a·pumpit3a 기동 스모크(기존 절차).

---

# Design: a shadow-authoritative segment-load slot (issue #18, direction 3)

## Grounding measurements

(1) On Win32 the physical segment registers never hold guest selectors —
a VEH resume ignores `SegEs` writes in the context (settled by
`repiu_aot_probe --segment-restore`). (2) The guarded-load slot's
physical-against-new compare therefore passes only for flat reloads;
guest-selector loads structurally always fall back. (3) pumpitea's
guest memcpy flips ES 0x0024↔0x002B on every call, both sides are
guarded **load** sites, and one memcpy costs two fallbacks, two ES
re-resolutions and two whole-cache re-patches by construction (4,420
each per 25 s). (4) Both selectors fold base 0, and native string ops
run on the physical flat segments, so a virtual switch inside a base-0
pair does not change the memory mapping — the same reasoning today's
HLE acceptance rests on. (5) All three attempts to cheapen the re-patch
path (v1–v3) lengthened the gap; keeping the trap and trimming its
aftermath is refuted, so removing the trap itself is what remains.

## Design

The shadow selector block becomes the source of truth for the guest's
virtual segment state, and the i386 guarded **load** slot writes
acceptable selector switches straight into it with no INT3 (Mermaid
flow above: new == [shadow] → pass; new == [pair0]/[pair1] → native
`[shadow] ← new`, pass; otherwise INT3 → the unchanged HLE load).

1. **Per-register accepted pair as memory words** in the shadow block:
   `pair[reg][0]` is the host flat selector (0x002B), written once at
   block creation; `pair[reg][1]` is updated by the HLE load whenever
   it accepts a base-0-folding, descriptor-backed non-flat selector
   (pumpitea's 0x0024), and cleared to 0 when DPMI changes that
   selector's base. The slot compares against the words, not
   immediates, so pair changes need no cache re-patch.
2. **New i386 slot** (layout above): the physical compare is dropped
   per (1); width and operand forms stay. `AotGuardedSegmentLoadSite`
   gains `pair0_address_offset`, `pair1_address_offset` and
   `shadow_store_offset`. The long-mode slot is left unchanged this
   time; the patcher keeps the old behavior when the new fields are 0.
3. **Inverted sync — guest_* from shadow**: `BuildAotSegmentTable`
   reads selectors from the shadow block when it exists and updates
   `context->guest_*` to match (today it pushes the other way); the
   block is seeded from `context->guest_*` at creation; and
   `SyncGuestSegmentsFromShadow(context)` runs at the HLE entries that
   consume `guest_*` before any re-resolve (`HandleDosInterrupt21`,
   `HandleDpmiInterrupt31`, the LINEXE glide boundary's segment
   consumers). `RecordGuestSegmentLoad` keeps writing both.
4. **Unchanged**: the HLE fallback's semantics (validation, descriptor
   registration, re-resolution and the unmasked capacity-wide
   re-patch — respecting the v1–v3 refutations); the override, pop,
   read and long-mode slots (the ISR's DS round trip at ~167/s is a
   later phase).

## Expected effect and risks

The flip's fallbacks, re-resolutions and re-patches (~8.8k each per
25 s) disappear from the loading gap, leaving the DOS time loops,
lseek and port I/O VEHs. Risk 1: collapsing the re-patch frequency may
interact with the still-unexplained v1–v3 paradox — judged only by the
measurement protocol. Risk 2: a missed `guest_*` consumer; the
inversion concentrates sync in `BuildAotSegmentTable` plus the named
service entries, and verification includes a consistency check.

## Verification

(1) `--selector-guard` updated and passing (new slot layout
assertions), `--segment-restore` still passing. (2) pumpitea 90 s under
the controlled protocol (cooled machine, exe copies, one discarded
warm-up, ≥2 interleaved baseline pairs), recording the gap, handled
segment loads, guarded success/fallback and re-patch counters.
(3) Conservative acceptance: selectors outside the pair (0x002C,
0x0090) still reach the HLE, checked with `REPIU_DPMI_SEGMENT_TRACE`.
(4) Regression fence: pumpit1/pumpit2a/pumpit3a startup smoke.
