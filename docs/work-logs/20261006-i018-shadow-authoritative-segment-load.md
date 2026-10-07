# 작업 로그: shadow 진실원 세그먼트 로드 슬롯 (issue #18, 방향 3)

작업 지시: `docs/work-orders/20261006-i018-shadow-authoritative-segment-load.md`
설계: `docs/design/20261006-i018-shadow-authoritative-segment-load.md`
분석: `docs/analysis/pumpitea-loading-segment-flip.md`

## 한 일

설계대로 구현했다.

1. `AotShadowSelectorBlock`에 레지스터별 `accepted_pair[6][2]` 추가,
   생성 시 flat 멤버 씨딩(`SeedAotShadowAcceptedPairs`)과 전체 selector
   씨딩.
2. i386 guarded load 슬롯 재설계: 물리 비교 제거(`--segment-restore`
   probe 근거), `[shadow]`/`[pair0]`/`[pair1]` 비교 + 쌍 일치 시
   `[shadow] ← 새 값` 네이티브 기록. 61바이트 슬롯, site에
   `pair0/pair1_address_offset`·`shadow_store_offset` 추가. long-mode
   슬롯은 그대로(새 필드 0 → 패처가 기존 동작).
3. 패처: 새 필드가 있으면 쌍 주소·기록 주소 패치, 쌍 주소가 없으면
   shadow 주소로 대체(기존 "no-op 재로드만 통과" 의미).
4. 엔진: `BuildAotSegmentTable` 역전(shadow→guest_* 동기,
   `SyncGuestSegmentsFromShadow`), 해석에 쌍 주소 포함,
   `SetGuestSegmentSelector` 헬퍼로 guest_* 직접 기록자들
   (get-vector, DOS4G FF00의 GS, LINEXE boundary ES) 전환,
   `HandleDosInterrupt21`·`HandleDpmiInterrupt31` 진입 동기,
   `ReadGuestSegmentSelector`가 shadow 블록을 읽음, HLE 로드가 base 0
   descriptor의 non-flat selector를 `pair[reg][1]`로 수용, DPMI
   AX=0007(set base) 시 해당 selector를 쌍에서 무효화, 동적 추가의
   오프셋 보정과 guard-fault 진단 창 확장.
5. probe: selector guard probe의 로드 슬롯 레이아웃·패치 단언을 새
   슬롯으로 갱신, 커버리지 검증기의 i386 로드 슬롯 단언 교체.

## 측정 (2026-10-06 저녁, 교대 프로토콜)

pumpitea 90초, 같은 디렉터리의 exe 사본 교대(기준선 = 같은 브랜치
HEAD 빌드):

| 순서 | 빌드 | 공백 | guarded load 성공/폴백 |
|---|---|---|---|
| 스모크 | 방향 3 | 7.59초 | 63,537 / 2,525 |
| 1 | 기준선 | 14.71초 | 72,585 / 6,873 |
| 2 | 방향 3 | **7.08초** | 73,178 / 2,525 |
| 3 | 기준선 | 15.64초 | 73,058 / 7,394 |
| 4 | 방향 3 | **6.77초** | 76,964 / 2,525 |

* **공백 절반 이하(14.7~15.6 → 6.8~7.6초)**, 방향 3의 분산이 작고
  guarded load 폴백이 2,525로 런 간 결정적이다. v0.0.180의 5.0초에
  근접했다.
* memcpy flip(레지스터/메모리-소스 `mov es, …`)의 INT3·재해석·전량
  재패치가 사라졌다. 재패치 빈도를 줄이는 이전 시도(v1~v3)와 달리
  역설이 나타나지 않았다 — 트랩 자체를 제거해 되먹임 고리의 모양이
  달라졌기 때문으로 본다.
* 남은 트랩: ISR의 메모리-소스 DS 로드(`mov ds, cs:[abs]`)와
  `pop ds`(guarded pop 폴백 48.7k/90초) — pop 슬롯의 쌍 수용과
  메모리-소스 로드 슬롯이 후속 단계 후보다.

## 검증

* `repiu_aot_probe --selector-guard` 통과(`selector_guard_all=true`,
  갱신된 레이아웃 단언 포함), `--segment-restore` 통과.
* 쌍 밖 selector가 HLE로 가는 것은 pumpit3a 트레이스에서 확인
  (0x0024/0x002B 메모리-소스 로드가 전부 HLE 경유).
* 기동 스모크(30초): pumpit1·pumpit2a 정상(예외 0, 프레임 정상).
  **pumpit3a는 4회 중 1회** 로딩 중 게스트 AV로 종료했다
  (`cmp dword [ebx+0xC], 0x1000`에서 EBX=0, EAX=0xDE1) — 오전 마스크
  v1 트레이스 런에서 1회 관측된 것과 동일 시그니처의 간헐 결함.
  재시도 3회와 30초 트레이스 런은 모두 정상이었고 기준선 1회도
  정상이었다. 이 간헐 크래시는 미해결로 남긴다(분석 문서의 미확정
  항목; 세그먼트 경로의 타이밍이 바뀔 때 드러나는 기존 경합으로
  추정).

---

# Work log: shadow-authoritative segment-load slot (issue #18, direction 3)

Implemented per the design: `accepted_pair[6][2]` in the shadow block
with flat-member seeding; the i386 guarded load slot re-emitted without
the physical compare, comparing `[shadow]`/`[pair0]`/`[pair1]` and
storing the shadow natively on a pair match (61-byte slot, three new
site offsets; long mode untouched); the patcher falling back to the
shadow address when pair addresses are absent; the engine inverted to
shadow-authoritative (`BuildAotSegmentTable` pulls,
`SyncGuestSegmentsFromShadow` at the DOS/DPMI entries,
`SetGuestSegmentSelector` for direct writers,
`ReadGuestSegmentSelector` reading the block, HLE acceptance of base-0
selectors into `pair[reg][1]`, DPMI set-base invalidation, dynamic-
append offset fixes, guard-fault window extension); and the probe and
coverage-validator assertions updated.

Interleaved 90 s pumpitea runs: baseline 14.71/15.64 s against
direction 3 at **7.59/7.08/6.77 s** with deterministic guarded-load
fallbacks (2,525) — the gap halves and approaches v0.0.180's 5.0 s, and
the speed-up-paradox did not reappear (the trap itself is gone, not
just its aftermath). Remaining traps are the ISR's memory-source DS
load and the guarded pop (48.7k fallbacks per 90 s), the next phase
candidates. Verification: both probes pass; pumpit1/pumpit2a smoke
clean; pumpit3a terminated once in four runs with the same intermittent
guest-AV signature first seen once this morning on the masked build
(EBX=0 at `cmp dword [ebx+0xC], 0x1000`), three retries and a traced
run clean — left unresolved in the analysis topic as a suspected
pre-existing race exposed by segment-path timing changes.
