# pumpitea 로딩 공백과 세그먼트 flip·재패치 결합 분석

issue #18의 로딩 공백(ANDAMIRO 로고 다음 프레임 없는 구간) 회귀를 쫓으며
확인한 사실을 누적한다. 측정은 모두 Win32 Release, `REPIU_STALL_TIMEOUT_MS=0`,
`REPIU_EXECUTION_TIMEOUT_MS=90000`, `REPIU_GLIDE_FRAME_RATE_LOG=1`, 창 보임,
vsync 기본에서 했다(2026-10-06, Intel HD 620 노트북).

## 확인됨

### ES flip의 정체: 게스트 memcpy 관용구

게스트 오프셋 `0xFE378`·`0xFE38A`(이미지 base `0x04000000`)의 ES 교대는
C 런타임 memcpy의 세그먼트 저장·복원이다. `repiu_aot_probe --dump 0x010FE360`
(probe base `0x01000000`)으로 디스어셈블했다.

```
0xFE36A  8B 7C 24 04   mov edi, [esp+4]
0xFE36E  89 D1         mov ecx, edx
0xFE370  8B 75 00      mov esi, [ebp+0]
0xFE373  8B 5C 24 08   mov ebx, [esp+8]
0xFE376  06            push es          ; guarded 지점 (INT3 폴백)
0xFE377  8C D8         mov ax, ds       ; 가상 DS = 0x0024
0xFE379  8E C0         mov es, ax       ; ES ← 0x0024 (flip 절반)
0xFE37B  57 89 C8 C1 E9 02 ...          ; rep movsd 준비
0xFE38A  8E 06?        mov es, [...]    ; ES ← 0x002B 복원 (flip 나머지)
```

`segment load trace`로 selector를 확인했다: `0xFE378`은 `0x0024`,
`0xFE38A`는 `0x002B`(source `0x049E0744`, 스택의 저장값). attract 구간에는
같은 모양의 **DS flip**(`0xFE2F0`: DS←0x0024 두 번, `0x2AB85`: DS←0x002B)이
이어진다. flip은 memcpy 호출마다 일어나므로 로딩 중 초당 약 1,100~2,400회다.

### 두 selector의 해석

`REPIU_AOT_SEGMENT_RESOLUTION_TRACE=1`로 확인한 ES 해석:

| selector | base | limit | 비고 |
|---|---|---|---|
| 0x002B | 0x00000000 | 0x0000FFFF | 호스트 flat, native fold |
| 0x0024 | 0x00000000 | 0x000F76BB | DOS 저메모리 한계 아래 → override 지점은 HLE 라우팅 |

재패치가 지점에 실제로 바꿔 쓰는 것은 가드의 selector 즉치값과
native/HLE 라우팅 형태뿐이다(base는 둘 다 0).

### v0.0.180이 이 경로에 들어오지 않았던 이유

v0.0.180의 Win32는 게스트의 DS를 가상으로 추적하지 않아, memcpy의
`mov ax, ds`가 물리 DS(호스트 flat `0x2B`)를 읽었고 `mov es, ax`는
현재 ES와 같은 값의 no-op 로드가 되어 guarded 지점이 네이티브로
성공했다(180 종료 요약: guarded load 성공 75,905 / 폴백 2,532).
Linux x64 이식(v0.0.181~191)에서 DS가 가상 selector(`0x0024`)로
추적되면서 `mov es, ax`가 실제 값 변경이 되었고 flip이 생겼다.
가상 추적이 실기 DOS/4GW(게임은 자신의 LE data selector로 돈다)에
더 충실하므로, 180의 동작은 우연한 지름길이었다.

### 캐시 전체 재패치는 순수한 낭비가 아니다 (issue 방향 1·2 기각)

issue #18의 방향 1(바뀐 레지스터의 지점만 재패치)과 2(쓰는 페이지만
`ProtectMemory`)를 구현해 측정했다. 같은 날 교대로 잰 90초 실행의
공백(span_ms 최대 간격):

| 빌드 | 공백(초) | handled segment load | guarded load 성공/폴백 |
|---|---|---|---|
| main(v0.0.205) 기준선 ×3 | **18.7 / 15.0 / 19.6** | 42.6k~47.3k | 47.5k~68.4k / 6.0k~7.3k |
| 마스크 v1 (override=해석 변화, guarded=shadow 변화) ×3 | **35.1 / 45.5 / 45.2** | 106k~118k | 12.5k / 40.8k |
| 마스크 v2 (둘 다 해석 변화) ×2 | **52.6 / 60.8** | 73.5k~77.2k | 25.7k~29.9k / 23.4k~25.5k |

기준선 3차(19.6초)는 v2 직후에 측정해 열 스로틀 교란을 배제했다.
마스크 변형은 둘 다 공백을 2~3배 **악화**시켰고, guarded 지점의
네이티브 성공이 무너졌다(기준선 ~90% 성공 → v1 ~23%). 재패치당
지점 기록 수는 1/5~1/12로 줄었는데도(종료 요약 selector guard 합산
210M → 45M) 벽시계 시간이 나빠졌으므로, 전량 재패치는 해석 갱신
이상의 일을 하고 있다.

### 로딩 구간은 예외 디스패치가 지배한다

30초 census(마스크 v1 빌드): 예외 총 296,966건 — single-step 2,281,
**breakpoint 209,722**, access-violation 23,716, 특권 명령(0xC0000096)
61,247. breakpoint 상위는 `0x04101297`(50,498), `0x04101BAE`(44,983),
`0x041012D3`(35,349), 그 다음이 flip 지점 `0xFE38A/0xFE378/0xFE376`
(각 16,741). 기준선 90초의 특권 명령 수(223,392)와 마스크 빌드
(227,277)는 비슷해, 차이는 breakpoint 쪽이다.

### 게스트 페이지 리타이어가 flip 블록 근처를 닫는다

두 빌드 모두 실행 초기에 `retire guard reset #1 page=0x04107000
guards=1 entries=1466`(generation 227) 한 번으로 **주소 맵 엔트리
1,466개가 INT3(0xCC)로 닫힌다**. `RetireAotGuestPage`는 엔트리의
`cache_offset`에 0xCC를 쓰고 비활성으로 표시한다.

## 추정

* 전량 재패치의 지점 prologue 복원(`PatchAot*Sites`가 매번
  `guard_prologue`를 다시 씀)이, 리타이어가 0xCC로 닫은 캐시 바이트를
  지점 단위로 **되살리는** 부수효과를 갖는다. 기준선은 flip마다
  (초당 ~1,000회+) 이 복원을 수행해 뜨거운 흐름이 네이티브로 돌고,
  마스크 변형은 바뀌지 않은 레지스터의 지점을 건너뛰어 닫힌 슬롯이
  영구 INT3 재진입으로 남는다. guarded 성공 붕괴(68k→12.5k)와 handled
  load 2.5배 증가가 이 가설과 일치한다. 단 v1보다 v2가 더 나빴던
  순서는 설명하지 못한다(측정 순서상 열 영향 가능).
* 이 복원은 리타이어된(스테일일 수 있는) 번역을 되살리므로 설계상
  건전하지 않다. 지금은 해당 페이지(0x04107000)의 코드가 실제로는
  변하지 않아 우연히 옳게 동작한다.

## 미확정

* 뜨거운 재진입 지점(`0x0410129x`, `0x04101BAE`)이 정확히 어떤 게스트
  루틴이고, 어떤 메커니즘(리타이어, HLE 라우팅, 안전점)으로 닫힌
  바이트에 계속 들어가는지. breakpoint provenance는 hle가 지배적이다.
* Win32에서 guarded load가 네이티브로 성공하려면 물리 segment
  레지스터가 게스트 selector와 같아야 하는데(슬롯은 물리 값과 비교),
  기준선에서 47k~68k가 성공하는 정확한 경위.
* 같은 코드(v0.0.205)의 공백이 issue 측정일에는 ~29초, 이날은
  15~19.6초로 달랐다. 일간 변동 요인(전원 계획, 드라이버, 백그라운드
  부하)은 통제하지 못했다.
* 마스크 v1 + `REPIU_AOT_SEGMENT_RESOLUTION_TRACE=1` 실행 한 번에서
  로딩 중 텍스처 경로의 게스트 AV(EIP 게스트 `0x041E4ECF`,
  `cmp dword [ebx+0xC], 0x1000`에서 EBX=0) 비정상 종료를 관측했다.
  트레이스 없는 3회에서는 재현되지 않았다.

## 다음 방향

1. **리타이어와 재패치의 결합을 명시적으로 풀기**: 전량 재패치의
   우연한 복원 대신, 리타이어된 엔트리로의 스테일 유입을 건전하게
   다시 잇는(재검증 후 재링크 또는 재번역 유도) 경로를 설계한다.
   그 전까지 전량 재패치를 줄이는 어떤 시도도 공백을 악화시킨다.
2. **flip 자체를 싸게**(issue 방향 3): memcpy 관용구의 세그먼트
   로드를 shadow 간 복사로 네이티브 처리하거나, 지점이 두 selector를
   기억하게 한다. flip HLE 11만 건(90초)과 그에 따른 재패치가 모두
   사라져야 180 수준(5초)에 접근한다.
3. 뜨거운 재진입 지점 3곳의 정체 규명이 1의 선행 작업이다.

---

# pumpitea loading gap: segment flip and re-patch coupling

Facts accumulated while chasing issue #18's loading-gap regression. All
measurements: Win32 Release, `REPIU_STALL_TIMEOUT_MS=0`,
`REPIU_EXECUTION_TIMEOUT_MS=90000`, `REPIU_GLIDE_FRAME_RATE_LOG=1`,
window visible, default vsync (2026-10-06, Intel HD 620 laptop).

## Confirmed

* **The ES flip is the C runtime memcpy idiom** at guest `0xFE376..0xFE38A`:
  `push es; mov ax, ds; mov es, ax; … rep movsd …; mov es, [saved]`.
  The virtual DS is `0x0024` and the saved ES `0x002B`, so every memcpy
  changes ES twice. The attract phase runs the same shape on DS
  (`0xFE2F0`, `0x2AB85`).
* **Both selectors fold base 0**: `0x002B` (host flat, native) and
  `0x0024` (limit `0xF76BB`, under the DOS low-memory bound, so override
  sites route to HLE). A re-patch changes only the guard immediates and
  the native/HLE routing form.
* **Why v0.0.180 avoided this**: 180's Win32 did not track a virtual DS,
  so `mov ax, ds` read the physical flat `0x2B` and the following
  `mov es, ax` was a no-op load that the guard passed natively (75,905
  successes / 2,532 fallbacks). The Linux x64 port made DS a tracked
  virtual selector (`0x0024`), which is more faithful to real DOS/4GW;
  180's behavior was an accidental shortcut.
* **The whole-cache re-patch is not pure waste** (issue directions 1 and
  2 are refuted). Same-day interleaved 90 s runs: baseline (v0.0.205)
  gaps **18.7 / 15.0 / 19.6 s**; masked v1 (override by changed
  resolution, guarded by shadow address) **35.1 / 45.5 / 45.2 s**;
  masked v2 (both by changed resolution) **52.6 / 60.8 s**. The third
  baseline ran right after v2, excluding thermal drift. Guarded-load
  native successes collapsed (baseline ~90% success rate → v1 ~23%) and
  handled segment loads grew 2.5x, although per-re-patch site writes
  dropped to 1/5–1/12.
* **The loading phase is exception-dispatch bound**: a 30 s census on
  the masked build counted 296,966 exceptions — 209,722 breakpoints,
  61,247 privileged (0xC0000096), 23,716 access violations. The top
  breakpoint sites are `0x04101297` (50,498), `0x04101BAE` (44,983),
  `0x041012D3` (35,349), then the flip trio at 16,741 each.
* **Guest-page retirement closes 1,466 address-map entries with INT3**
  early in both builds (`retire guard reset #1 page=0x04107000
  entries=1466`, generation 227).

## Inferred

* The full re-patch's prologue restore resurrects, site by site, cache
  bytes that retirement closed with INT3; the baseline performs this
  about a thousand times a second, keeping the hot flows native, while
  the masked variants skip unchanged registers' sites and leave the
  closed slots in permanent INT3 re-entry. The guarded-success collapse
  and the 2.5x handled-load growth match; the v2-worse-than-v1 ordering
  remains unexplained (possibly thermal, by measurement order).
* That restoration revives translations retirement had closed, so it is
  unsound by design and only accidentally correct today (the code on
  that page does not actually change).

## Unresolved

* The identity of the three hot re-entry sites and which mechanism's
  closed bytes they keep entering (provenance is dominated by `hle`).
* How guarded loads succeed natively on Win32 at all, given the slot
  compares the physical segment register.
* Day-to-day variance: the same v0.0.205 code measured ~29 s in the
  issue and 15–19.6 s on this day.
* One crash under `REPIU_AOT_SEGMENT_RESOLUTION_TRACE=1` on masked v1
  (guest AV at `0x041E4ECF`, `cmp dword [ebx+0xC], 0x1000` with EBX=0,
  during texture loading); not reproduced in three runs without the
  trace.

## Next directions

1. Decouple retirement from the re-patch explicitly: a sound re-link or
   re-translation path for stale inflow into retired entries. Until
   then, any reduction of the whole-cache re-patch worsens the gap.
2. Make the flip itself cheap (issue direction 3): native shadow-to-
   shadow handling of the memcpy idiom's segment loads, or two
   remembered selectors per site. Only removing the ~110k flip HLEs per
   90 s (and their re-patches) can approach 180's 5 s.
3. Identifying the three hot re-entry sites is the prerequisite of 1.
