# Megamorphic direct-return table 작업 로그

설계: [20260822-499-megamorphic-direct-return-table.md](../design/20260822-499-megamorphic-direct-return-table.md)

작업 지시: [20260822-499-megamorphic-direct-return-table.md](../work-orders/20260822-499-megamorphic-direct-return-table.md)

측정 절차: [return stage 귀속 가이드](../guides/return-stage-attribution.md)

## 1. 무엇을 만들었나

megamorphic return이 host로 넘어가기 전에 생성 코드가 직접 읽는 **공용 memo table**을
추가했습니다. 적중하면 host 전환 없이 원본 RET과 같은 효과로 돌아가고, 실패하면 기존
경로가 **한 바이트도 바뀌지 않은 채** 이어집니다.

| 파일 | 내용 |
|---|---|
| `include/repiu/runtime/aot_direct_return_table.h` · `src/runtime/aot_direct_return_table.cpp` | 항목·해시·삽입·조회·무효화, `REPIU_AOT_DIRECT_RETURN_TABLE_BITS` 해석 |
| `aot_code_cache.{h,cpp}` | probe emitter와 operand 패처, build option, probe site 목록, guard target 헬퍼 |
| `aot_code_cache_win32.{h,cpp}` | table 소유, 배치·동적 append의 operand 패치와 offset 재보정, 패처의 guard target |
| `aot_page_coherence_win32.cpp` | retirement 시 table 전체 무효화, guard reset target |
| `aot_runtime_dispatch.cpp` | 검증된 active-hit resolution만 table에 기록 |
| `main.cpp` · `live_telemetry_snapshot.cpp` · `execution_trampoline.h` | opt-in 환경 변수와 종료 요약 |
| `src/tools/aot_probe/aot_direct_return_table_probe.{h,cpp}` | 합성 probe 5항목, `--direct-return-table` |

## 2. 구현에서 갈린 판단들

### 2.1 probe는 miss tail **앞**에 두고, `miss_cache_offset`은 건드리지 않았습니다

처음에는 probe를 miss 지점 뒤에 두려 했지만, 그러면 adapter의
`kFallbackFromMissBytes = 16`(miss에서 fallback까지의 고정 거리)이 깨집니다. 반대로
`miss_cache_offset` 자체를 probe 시작으로 옮기면 **세 소비자가 동시에 깨집니다** —
push되는 miss address, adapter의 site 조회 키, 그리고 `IsAotInlineCacheMiss`가 쓰는 Task
479 인덱스가 모두 그 값을 키로 씁니다.

그래서 probe를 앞에 두되 `miss_cache_offset`은 그대로 popfd를 가리키게 하고, **PIC
guard만** probe를 향하게 했습니다. guard target은 세 곳(emitter, patcher, retirement의
guard reset)에서 쓰이므로 `AotInlineCacheGuardTargetOffset()` 하나로 모았습니다. probe가
없는 site에서는 이 함수가 기존 값을 그대로 돌려주므로 indirect 경로는 무변화입니다.

이 판단이 중요한 이유: guard reset과 patcher가 예전 값을 계속 쓰면, 한 번 패치된
site는 **probe를 영원히 건너뜁니다**. 즉 megamorphic이 되는 바로 그 site들만 조용히
기능을 잃습니다.

### 2.2 적중 결과는 전역이 아니라 스택으로 넘깁니다

적중 시 cache target을 게스트 return 슬롯에 덮어쓰고 원본과 같은 `RET`으로 점프합니다.
전역 스크래치 워드를 썼다면 host가 비동기로 주입하는 타이머 인터럽트(Task 294)가 store와
jump 사이에 끼어들어 값을 덮을 수 있습니다. 스택으로 넘기면 그 창 자체가 없습니다.

### 2.3 정확성은 새 불변식이 아니라 기존 것에 얹었습니다

table 적중은 4-entry PIC 적중과 **의미가 같습니다** — 둘 다 host, call-depth
bookkeeping, telemetry, trap flag 정리를 건너뜁니다. 그래서 검증할 것은 "항목이 PIC
항목과 같은 조건에서만 살아 있는가" 하나로 줄었고, 규칙도 둘뿐입니다.

* **삽입:** host가 `ResolveAotTransferTarget`으로 검증한 결과 중 `kActiveHit`만.
  Glide gate direct target과 dynamic translation 결과는 다른 resolution을 보고하므로
  자동으로 빠집니다.
* **무효화:** `ResetInlineCacheGuardsTargetingPage`가 PIC guard를 되돌리는 바로 그
  자리에서 table 전체를 지웁니다. 그 지점은 worker 스레드지만
  `RequestWin32AotGuestPageRetirement`가 게스트 스레드를 `INFINITE`로 **정지시킨 채**
  요청하므로, lock도 generation stamp도 필요 없습니다.

### 2.4 hit 카운터를 생성 코드에 넣었습니다 (설계에서 추가)

설계에는 없던 `inc dword ptr [counter]` 한 줄을 적중 경로에 넣었습니다. 회당 1~2 cycle
비용으로 "생성 코드가 실제로 host를 건너뛰었다"는 **직접 증거**가 남습니다. 이 값이 0인
채로 성능만 좋아 보이는 상황을 배제할 수 없다면 A/B를 해석할 수 없습니다.

## 3. 검증

* Win32 x86 **Debug/Release** `repiu_aot_probe`와 `repiu` 빌드 성공, 신규 오류 없음
  (기존 C4819 경고만).
* `repiu_aot_probe --direct-return-table`: 5항목 전부 `true`.
  * `bits` — unset·빈 값·비숫자·범위 밖 값의 clamp와 기본값.
  * `accounting` — 삽입·갱신·충돌 덮어쓰기, zero key/target 거부, 조회 oracle,
    무효화 후 조회 실패, reset의 크기 clamp.
  * `emission_gate` — **꺼짐이면 아무것도 emit하지 않고**(`direct_return_probe_sites`
    비어 있고 `miss_probe_cache_offset == 0`), 켜짐이면 probe가 miss tail 앞에 놓이며
    `miss_cache_offset`은 여전히 popfd를 가리키고 dispatch site가 같은 값을 키로 씁니다.
  * `operands` — mask·key·value·counter 절대 주소 패치와 잘린 버퍼 거부, `C2 iw` 형태의
    길이 차이.
  * `execution` — **emit한 바이트열을 실제로 실행합니다.** 적중 시 landing pad에
    도달하고 `ESP`가 원본 RET과 정확히 같으며 `eax`/`ecx`가 보존되고 hit 카운터가 1
    증가합니다. table이 비면 miss tail로 떨어집니다. `C3`와 `C2 4` 두 형태 모두.
* `repiu_aot_probe build/runtime_mounts/pumpit8/PIU/PIU.EXE` 전체 단정 **exit 0**,
  `inline_cache_all`·`return_patch_policy_all`·`return_stage_all`·`coherence_all` 유지.
* **꺼짐이 진짜 무변화임을 이미지 수준에서 확인:** 같은 pumpit8 이미지의
  `cache_bytes=425108`, `cache_map_entries=68698`, `cache_fixups=19828`,
  `cache_resolved_fixups=19547`가 Task 482 시점 로그와 **완전히 동일**합니다.

### 3.1 실행 스모크 1회 — 켠 상태가 실제로 동작합니다 (판정 아님)

Release `repiu.exe pumpit8`, wall 60초, vsync OFF, EEPROM 격리,
`REPIU_AOT_DIRECT_RETURN_TABLE=1` + `REPIU_EXECUTION_TIME_PROFILE=1`. **무인 실행이라
장면이 pass4~6과 다르므로 성능 판정에는 쓰지 않습니다.**

```
Win32 AOT direct-return table enabled/sites/entries/hits/share/inserts/overwrites/clears:
    true/6989/8192/90338821/99.76%/214750/211909/37
```

**구조적 사실 (장면과 무관).**

| 지표 | pass4 (기능 없음) | 스모크 (켬) |
|---|---:|---:|
| host return dispatch | 49,554,406 | **214,790** (−99.57%) |
| `kAotReturn` bucket | 53,904,358,258 = `guest-run`의 25.63% | **776,512,927 = 0.35%** |
| inline cache 패치 | 293,562 | 1,784 |

생성 코드가 **90,338,821회**의 return을 스스로 해결했고, host로 넘어간 것은 214,790회
(0.24%)뿐입니다. return 처리 비용은 `guest-run`의 25.63%에서 **0.35%**로 떨어졌습니다.

**건전성.** return fallback 전 항목 0, site index `scans=0`, quarantine 이벤트 0,
warning·critical 0건, Glide 구현 이슈 0건. 종료는 wall 예산 만료입니다.

**무효화 경로가 실제로 돌았습니다.** 이 실행에서 page retirement가 37회 발생해 table을
37번 비웠고(`clears=37`), 그때마다 재학습하면서도 fallback 0을 유지했습니다. 합성
probe가 아니라 실제 SMC 활동 아래에서 무효화 규칙이 검증된 셈입니다.

**참고값 (인용 금지).** cycle당 프레임은 pass4의 6,309,570에서 3,946,127로 낮아졌고
Glide gate 비중은 24%대에서 39.7%로 올랐습니다. 후자는 return에서 풀려난 CPU가 다른 곳을
쓰고 있다는 뜻으로 읽히지만, **장면이 다른 1회 실행이므로 성능 근거가 아닙니다.**

**다음 실험 신호.** 삽입 214,750건 중 **211,909건(98.7%)이 덮어쓰기**입니다. clear 1회당
약 5,800개 항목을 재학습하는데 8,192 슬롯에서 충돌이 잦다는 뜻이므로,
`REPIU_AOT_DIRECT_RETURN_TABLE_BITS=15`가 남은 214,790회 host 왕복을 더 줄이는지 보는
것이 가장 싼 다음 실험입니다.

### 3.2 table 크기 실험 — 8,192는 thrash 중이었습니다 (기본값 13 → 15)

3.1의 덮어쓰기 98.7%를 확인하러 같은 조건에서 `REPIU_AOT_DIRECT_RETURN_TABLE_BITS=15`로
한 번 더 돌렸습니다.

| 지표 | bits=13 (8,192) | bits=15 (32,768) |
|---|---:|---:|
| 적중 / 적중률 | 90,338,821 / 99.76% | 90,672,808 / **100.00%** |
| 삽입 / 덮어쓰기 | 214,750 / 211,909 | **2,892 / 33** |
| host return dispatch | 214,790 | **2,932** (−98.6%) |
| `kAotReturn` bucket | 776,512,927 = 0.35% | **294,677,665 = 0.13%** |
| table clear | 37 | 37 |

**8,192 슬롯이 부족한 것이 맞았습니다.** 작업 집합이 약 5,800개인데 덮어쓰기가 21만 건
발생했고, 덮어쓰기 하나는 곧 나중의 host 왕복 하나입니다. 32,768에서는 같은 장면의
덮어쓰기가 **33건**으로 떨어지고 host 왕복은 사실상 사라집니다.

상주 footprint는 두 경우가 같습니다 — 실제로 건드리는 항목만 캐시 라인을 차지하므로,
큰 table은 서로 다른 target이 충돌하지 않게 할 뿐입니다. 그래서 기본값을 **15**로
올렸습니다.

**주의: 이 실험은 처리량을 개선하지 않았습니다.** cycle당 프레임은 3,946,127 →
4,019,542로 오히려 1.9% 나빠졌는데, 두 값 모두 단일 무인 실행이라 잡음 범위입니다.
0.35%에서 0.13%로 줄인 몫은 애초에 보이지 않는 크기입니다. **return 축은 이 시점에서
닫혔다고 읽는 것이 맞습니다** — 같은 실행에서 Glide gate가 `guest-run`의 40.7%로
지배적입니다.

건전성은 bits=15에서도 동일합니다: fallback 0, `scans=0`, quarantine 0, warning 0,
정상 종료.

### 3.3 성능 판정 A/B — ON/OFF 각 3회, 프레임 +59.4%

계측 토글을 **모두 끄고**(`REPIU_EXECUTION_TIME_PROFILE` 없이) 기능 토글만 바꿔
OFF/ON을 교대로 6회 돌렸습니다. 각 실행은 wall 60초 고정, vsync OFF, 같은 fixture에서
복사한 EEPROM으로 시작합니다. 무인 실행이므로 장면이 재현 가능합니다.

| 실행 | 프레임 | tri/frame | patches/frame | host return dispatch | table 적중률 |
|---|---:|---:|---:|---:|---:|
| off-1 | 37,950 | 101.66 | 9.85 | 57,986,539 | — |
| off-2 | 37,500 | 101.28 | 9.89 | 57,230,742 | — |
| off-3 | 36,706 | 101.32 | 10.00 | 56,026,352 | — |
| on-1 | 58,761 | 104.06 | 0.03 | 2,930 | 100.00% |
| on-2 | 59,566 | 104.47 | 0.03 | 2,916 | 100.00% |
| on-3 | 60,432 | 104.36 | 0.03 | 2,991 | 100.00% |

**판정: 통과.** 60초 예산에서 프레임이 평균 **37,385 → 59,586, +59.38%**입니다
(623 fps → 993 fps). 실행 간 편차는 OFF 1.38%, ON 1.15%이고 **두 집단의 범위가 전혀
겹치지 않습니다**(최저 ON 58,761 > 최고 OFF 37,950).

**장면 동일성.** 프레임당 삼각형이 101.42 → 104.30으로 **+2.84%, 3% 규칙 안**입니다.
`grDrawPoint`는 여섯 실행 모두 **정확히 1,411,200**으로 동일합니다. 즉 ON은 같은 내용을
더 빨리 그린 것이며, 프레임당 작업량은 오히려 소폭 많습니다 — 그만큼 +59.4%는 보수적인
값입니다.

**프레임당 패치 3% 규칙은 이 변경에 적용할 수 없습니다.** 9.92 → 0.03(−99.7%)은 장면
차이가 아니라 **이 기능의 목적 그 자체**입니다(host 왕복을 없애면 패치도 사라집니다).
Task 478이 그 규칙을 만들 때 전제한 "패치 경로를 건드리지 않는 변경"이 아니므로, 여기서는
tri/frame과 point 총량을 장면 guard로 씁니다. Task 481도 같은 이유로 장면 비교가
무효였던 전례가 있습니다.

**건전성.** 여섯 실행 모두 return fallback 0, `scans=0`, 정상 종료. ON 3회 모두 적중률
100.00%.

3.2에서 "처리량은 좋아지지 않았다"고 적은 것은 **계측을 켠 단일 실행 비교였기
때문**입니다. 계측을 빼고 3회씩 재보니 효과가 분명합니다.

### 3.4 승격 확인 — 기본 ON과 명시적 opt-out 양방향

`ResolvePromotedToggle`로 올린 뒤 30초씩 두 번 확인했습니다.

| | 환경 변수 없음 | `REPIU_AOT_DIRECT_RETURN_TABLE=0` |
|---|---|---|
| table | `true`, 32,768 항목, 적중 45,093,348(99.99%) | `false`, 0 항목 |
| host return dispatch | 2,918 | 23,912,620 |
| 캐시 바이트 | 526,083 | **473,078** |
| 프레임(30초) | 29,217 | 16,193 |

**`=0`이 기능 도입 전 캐시 바이트(473,078)를 정확히 복원합니다.** 두 실행 모두 fallback
0, warning 0, 정상 종료입니다.

캐시 용량은 여유가 큽니다 — capacity 16 MiB에 ON 이미지가 526 KB로 OFF 대비 +11.2%입니다.

## 4. 남은 것

* **기본값 승격 완료.** 3.3의 A/B를 근거로 `ResolvePromotedToggle`로 올렸습니다(미설정은
  ON, 명시적 `0|off|false`만 OFF). 캐시 용량은 문제되지 않습니다 — capacity가 16 MiB인데
  ON 이미지는 526,083 B로 OFF의 473,078 B 대비 +11.2%입니다.
* **남은 확인:** 승격 근거는 pumpit8 무인 attract 장면 하나이므로, 실제 게임플레이
  장면에서 프레임과 `AOT direct-return table` 적중률을 한 번 확인해 두는 것이 좋습니다.
  회귀가 보이면 `REPIU_AOT_DIRECT_RETURN_TABLE=0`이 즉시 이전 동작으로 되돌립니다.
* 적중률이 낮게 나오면 `REPIU_AOT_DIRECT_RETURN_TABLE_BITS`를 올려 충돌을 줄이는 것이
  첫 대응입니다. 종료 요약의 `overwrites`가 그 판단 근거입니다(3.2절이 실제 사례).
* **다음 축은 Glide입니다.** return이 0.13%로 내려간 실행에서 Glide gate가 40.7%입니다.
  Task 482 pass 1이 지목한 두 후보 — elision 판정을 crossing 앞으로(약 11%), 호스트
  wake 지연(약 6%) — 가 이제 1순위입니다.

---

# Megamorphic Direct-Return Table Work Log

Design: [20260822-499-megamorphic-direct-return-table.md](../design/20260822-499-megamorphic-direct-return-table.md)

Work order: [20260822-499-megamorphic-direct-return-table.md](../work-orders/20260822-499-megamorphic-direct-return-table.md)

## 1. What was built

A shared memo table that generated code probes before a megamorphic return crosses to the host.
A hit returns with exactly the original RET's effect and no host transition; a miss falls through
into the existing path with not one byte of it changed. New platform-neutral
`aot_direct_return_table.{h,cpp}` own the entries, hash, insertion, lookup, invalidation, and the
`REPIU_AOT_DIRECT_RETURN_TABLE_BITS` policy; the emitter gained the probe and its operand patcher;
the Win32 placement owns the table and patches the absolute operands at placement and on dynamic
append; retirement clears the table; the return handler records validated mappings; and a
synthetic probe covers five cases behind `--direct-return-table`.

## 2. Judgement calls

**The probe goes ahead of the miss tail, and `miss_cache_offset` never moves.** Putting the probe
after the miss point would break the adapter's fixed `kFallbackFromMissBytes = 16` distance, while
moving `miss_cache_offset` to the probe would break three consumers at once: the pushed miss
address, the adapter's site lookup key, and the Task 479 index `IsAotInlineCacheMiss` uses. So the
probe sits ahead of it and only the PIC guards point at the probe, through one
`AotInlineCacheGuardTargetOffset()` helper shared by the emitter, the patcher, and the retirement
guard reset. That last part matters: had the patcher and the reset kept using the old value, any
site that was ever patched would silently skip its probe forever — precisely the sites that go
megamorphic.

**The hit result travels on the stack, not through a global.** The probe overwrites the guest
return slot and executes the original RET. A global scratch word would leave a window in which an
asynchronously injected timer interrupt (Task 294) could overwrite it between the store and the
jump.

**Correctness rests on an existing invariant rather than a new one.** A table hit is semantically
identical to a four-entry PIC hit, so the only obligation is that an entry lives exactly as long
as a PIC entry would. Insertion takes only `kActiveHit` resolutions, which excludes Glide-gate
direct targets and freshly translated code automatically; invalidation clears the whole table at
the same point that resets PIC guards, where the guest thread is already blocked for the
retirement rendezvous, so no lock and no generation stamp are needed.

**A hit counter was added to the generated code, which the design did not have.** One
`inc dword ptr [counter]` on the hit path, one or two cycles, buys direct evidence that generated
code actually skipped the host. Without it an A/B could not distinguish a working table from a
dead one that happened to run faster.

## 3. Verification

One 60-second unattended Release smoke on pumpit8 with the feature on confirmed it works in a
real run: generated code resolved **90,338,821 returns itself, a 99.76% share**, leaving 214,790
host dispatches against pass4's 49,554,406, and the `kAotReturn` bucket fell from 25.63% of
`guest-run` to **0.35%**. Return fallbacks, index scans, quarantine events, warnings, and Glide
issues were all zero. Thirty-seven page retirements cleared the table during the run and it kept
resolving correctly afterwards, so the invalidation rule was exercised under real self-modifying
code rather than only in the probe. The scene differs from the user's runs, so none of the
throughput numbers are performance evidence. One follow-up signal: 211,909 of 214,750 inserts were
overwrites, so a second smoke ran the same scene at
`REPIU_AOT_DIRECT_RETURN_TABLE_BITS=15`. The 8,192-entry table had indeed been thrashing: at
32,768 entries the same scene produced **33 overwrites instead of 211,909**, a 100.00% hit share,
and host return dispatches fell from 214,790 to **2,932**, taking the `kAotReturn` bucket from
0.35% of `guest-run` to 0.13%. The resident footprint is the same either way, since only touched
entries occupy cache lines, so the default is now fifteen bits. That experiment did **not** improve
throughput -- cycles per frame moved 1.9% the wrong way, within the noise of single unattended runs
-- which is the honest reading that the return axis is now closed: in the same run the Glide gate
holds 40.7% of `guest-run`.

**A/B judgement: passed.** With every instrument off and only the feature toggle changing, three
alternating pairs of 60-second runs gave 37,950 / 37,500 / 36,706 frames with the table off
against 58,761 / 59,566 / 60,432 with it on -- a mean of 37,385 against 59,586, **+59.38%**, or
623 against 993 frames per second. Run-to-run spread was 1.38% and 1.15%, and the two groups do
not overlap at all. The scene held: triangles per frame moved 101.42 to 104.30, +2.84% and inside
the 3% rule, and `grDrawPoint` was exactly 1,411,200 in all six runs, so the enabled arm drew the
same content slightly faster per frame and far more often. The 3% patches-per-frame guard does not
apply here, because 9.92 to 0.03 is the feature's own purpose rather than a scene difference --
removing host round trips necessarily removes the patches -- the same situation that invalidated
Task 481's scene comparison. All six runs ended normally with zero return fallbacks and zero index
scans, and all three enabled runs reported a 100.00% hit share.

Win32 x86 Debug and Release builds of `repiu_aot_probe` and `repiu` succeeded with no new errors.
`repiu_aot_probe --direct-return-table` passed all five checks: bits resolution with clamping,
table accounting including collision overwrite and the zero-key convention, the emission gate
(nothing at all emitted while off; while on, the probe precedes a miss tail whose
`miss_cache_offset` still points at the popfd the dispatch site keys on), operand patching with
bounds refusal, and **execution of the emitted bytes** — a hit reaches the landing pad with ESP
exactly as the original RET would leave it, `eax` and `ecx` preserved, and the hit counter
incremented, while an empty table falls through to the miss tail, both for `C3` and for `C2 4`.
The complete pumpit8 probe exited zero with the existing assertion groups unchanged, and the
disabled build is identical at the image level: `cache_bytes=425108` and the same map, fixup, and
resolved-fixup counts as the Task 482 log.

## 4. Remaining

The feature has not been run live yet. A run with it on should show hits and their share in the
`Win32 AOT direct-return table` summary line while return fallbacks and index scans stay at zero.
Promotion follows the guide's rule — the same section three times, per-frame patches and
primitives within 3%, swaps and primitives per cycle moving together. If the hit rate disappoints,
raising `REPIU_AOT_DIRECT_RETURN_TABLE_BITS` to cut collisions is the first response, and the
summary's `overwrites` count is the evidence for that call.
