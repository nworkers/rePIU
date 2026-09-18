# Task 712 작업 로그 — flat 스택 선택자의 명시적 SS override

설계: [20260918-712](../design/20260918-712-flat-stack-ss-override.md) ·
작업 지시: [20260918-712](../work-orders/20260918-712-flat-stack-ss-override.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-711](20260918-711-piu-bin-loop-ss-override.md)

## 수행 결과

loader의 초기 스택 선택자 아래에서 명시적 `SS:` override가 base 0으로 fold됩니다.
원칙은 **명시적 `SS:` 접근은 같은 SS 아래의 암묵적 스택 접근과 같은 주소로 간다**는
것입니다. loader의 스택에서는 `ESP`가 linear이므로 실효 base가 0이고, 게스트가
스스로 만든 스택(Task 692의 `B4`, base `0x0158A83C`)에서는 descriptor base가
그대로입니다.

* `runtime::ApplyFlatStackSegmentFold` — 초기 스택 선택자이고 정책이
  `kNativeFolded`인 SS 항목만 base를 0으로 둡니다. 선택자 0, DOS low-memory,
  미상(`0`)은 건드리지 않습니다.
* `ThreadContext::flat_stack_selector` — 실행 시작 때 `guest_initial_ss`로 정합니다.
* `BuildAotSegmentTable` — fold 표를 만드는 유일한 곳에서 적용합니다. Task 289의
  fingerprint 비교가 이 함수의 결과끼리 비교하므로 재해석이 진동하지 않습니다.
* 선택자 표의 descriptor는 바꾸지 않았습니다. DOS allocator와 mode16 경로가 그
  base를 다른 목적으로 읽습니다.

## 검증

### probe

* Linux x64 core probe **30/30**, Win32 x86 core probe **28/28**
* 새 `flat_stack_segment_fold` group: 초기 스택 선택자에서 base 0, 게스트 스택
  `B4`에서 base 유지, DOS low-memory 유지, 미상 선택자에서 무변화, null 안전

이 규칙은 이번에 처음 생긴 함수라 "수정 전 구현에서 probe가 실패한다"는 보일 수
없습니다. before/after 증거는 아래 실행입니다.

### Linux x64 (30초, `pumpit2a`)

| | Task 711 | Task 712 | Win32 (712 후) |
|---|---:|---:|---:|
| DOS read | 1,110,356 | **91** | 91 |
| DOS seek | 24 | **25** | 25 |
| DOS open | 11 | 11 | 11 |
| 폴트 | 0 | 0 | 0 |
| Glide gate #51 | `_GRALPHACOMBINE@20` | `_GRALPHACOMBINE@20` | `_GRTEXTEXTUREMEMREQUIRED@8` |

**두 host의 파일 연산이 이제 정확히 같습니다**(91 / 25 / 11). gate #51은 그대로이고,
그것은 Task 713의 대상입니다.

### Win32 회귀 관찰

같은 바이너리 구성과 같은 조건(`REPIU_STALL_TIMEOUT_MS=0`,
`REPIU_EXECUTION_TIMEOUT_MS=30000`, `REPIU_GLIDE_DRAW_DIAG=1`)으로 수정 전후를
비교했습니다.

| 항목 | 전 | 후 | 판정 |
|---|---|---|---|
| PIU.BIN 복원 seek | `0x0458CC60` (쓰레기) | **`0`** | 수정의 정당한 결과 |
| PIU.BIN 첫 read | 4096 요청 → **0바이트**, 반복 | 4096 요청 → **560바이트**(파일 전체) | 수정의 정당한 결과 |
| DOS read / seek / open | 122 / 25 / 11 | 91 / 25 / 11 | 정당 — 0바이트 재시도가 사라짐 |
| Glide gate 96개(ordinal·반환 주소) | — | **차이 0** | 회귀 없음 |
| LFB unlock | 219,918 / 614,400 | **같음** | 회귀 없음 |
| 삼각형 12개의 좌표·s/t·combine·texEnabled | — | **같음** | 회귀 없음 |
| 삼각형 12개의 non-black 픽셀 | 0, 29,919, 29,928 | **같음** | 회귀 없음 |
| 삼각형 정점 색 dword 2–7 | 전부 0 | `A1A10000 50500100 08FF20FF …` | 아래 참조 |
| frames / 폴트 | 1,021 / 0 | 1,017 / 0 | 실행 간 편차 범위 |

**Win32에서 PIU.BIN 레코드 테이블이 처음으로 채워집니다.** 이전에는 복원 seek가
쓰레기 offset으로 가서 35번의 `fread`가 전부 0바이트였습니다. Task 711의 추정이
측정으로 확인됐습니다.

**설명되지 않은 차이 하나.** 첫 삼각형 정점 버퍼(`0x04800F38`)의 색 dword가 0에서
반복되는 바이트 패턴으로 바뀌었습니다. 같은 패턴이 여러 정점과 위치에 나타나므로
게임이 쓴 정점 색이 아니라 **게임이 초기화하지 않은 필드에 남은 메모리**로
추정합니다. PIU.BIN이 읽히면서 힙 사용이 달라진 결과일 것입니다. 이 필드가 화면에
영향을 주는지는 픽셀로 판정했고, 측정한 12개 draw에서는 **영향이 없었습니다.**
다만 측정 범위는 처음 12개 draw뿐이며, 모든 장면에서 영향이 없다고 확인한 것은
아닙니다.

## 남은 것

1. **텍스처 업로드 생략**(Task 713). 파일 연산이 두 host에서 같아졌는데도 gate #51이
   갈라지므로, 원인은 파일 쪽이 아닙니다.
2. **DS/ES override.** 초기 DS `0x0024`도 base `0x01010000`이 있어 같은 결함이 있을
   수 있지만 측정하지 않았습니다.

---

## English

Design: [20260918-712](../design/20260918-712-flat-stack-ss-override.md) ·
Work order: [20260918-712](../work-orders/20260918-712-flat-stack-ss-override.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-711](20260918-711-piu-bin-loop-ss-override.md)

### Result

An explicit `SS:` override now folds base 0 under the loader's initial stack
selector. The principle is that **an explicit `SS:` access reaches the same address
as an implicit stack access under the same SS**: on the loader's stack `ESP` is
linear, so the effective base is 0, while a stack the guest builds itself (Task
692's `B4`, base `0x0158A83C`) keeps its descriptor base. The pure rule is
`runtime::ApplyFlatStackSegmentFold`, which touches only a natively folded SS entry
on the initial stack selector; `ThreadContext::flat_stack_selector` is set from
`guest_initial_ss`; and the rule is applied in `BuildAotSegmentTable`, the one place
the fold table is built, so Task 289's fingerprint comparison compares like with
like. The selector table's descriptors are unchanged, since the DOS allocator and
the mode16 paths read that base for other purposes.

### Verification

Linux x64 core probe **30 of 30** and Win32 x86 **28 of 28**. The new
`flat_stack_segment_fold` group checks base 0 on the loader's stack, the base kept
on the guest-built `B4` stack, DOS low memory kept, no change for an unknown
selector, and null safety. The rule is a new function, so the probe cannot be shown
failing against a pre-fix implementation; the before/after evidence is the runs.

On a 30-second Linux x64 run, DOS reads fell from 1,110,356 to **91** and seeks
became **25**, with 11 opens and no faults. **File activity is now identical on the
two hosts** (91 / 25 / 11). Gate #51 is unchanged; that is Task 713.

**Observing Win32 regression.** A before/after comparison under identical
conditions (`REPIU_STALL_TIMEOUT_MS=0`, `REPIU_EXECUTION_TIMEOUT_MS=30000`,
`REPIU_GLIDE_DRAW_DIAG=1`):

* The PIU.BIN restore seek went from `0x0458CC60` (garbage) to **`0`**, and the
  first read now returns **560 bytes**, the whole file, instead of repeated 0-byte
  reads. **PIU.BIN's record table is filled on Win32 for the first time**, which
  confirms Task 711's inference by measurement. Reads fell from 122 to 91 with
  seeks and opens unchanged.
* **No regression** in all 96 Glide gates (ordinal and return address), the LFB
  unlock (219,918 of 614,400 bytes), the coordinates, s/t, combine and
  texture-enable of the 12 sampled triangles, or their non-black pixel counts
  (0, 29,919, 29,928). Frames 1,021 → 1,017 with no faults is within run-to-run
  variance.
* **One difference remains unexplained.** Vertex color dwords 2–7 in the first
  triangle's buffer (`0x04800F38`) changed from zero to repeating byte patterns such
  as `A1A10000 50500100 08FF20FF`. The same pattern recurs across vertices and
  positions, so it is inferred to be leftover memory in fields the game does not
  initialize — presumably because heap use changes once PIU.BIN is read — rather
  than colors the game wrote. Judged by pixels, it had **no effect** on the 12
  sampled draws; only those 12 were measured, so no effect across every scene is
  not established.

### What is left

1. **The skipped texture upload** (Task 713). File activity is now identical on both
   hosts and gate #51 still diverges, so the cause is not on the file side.
2. **DS/ES overrides.** The initial DS `0x0024` also has a base (`0x01010000`) and may
   share the defect; not measured.
