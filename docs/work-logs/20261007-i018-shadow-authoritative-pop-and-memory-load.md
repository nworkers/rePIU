# 작업 로그: shadow 진실원 pop 슬롯과 메모리-소스 로드 슬롯 (issue #18, 방향 3 2단계)

작업 지시: `docs/work-orders/20261007-i018-shadow-authoritative-pop-and-memory-load.md`
설계: `docs/design/20261007-i018-shadow-authoritative-pop-and-memory-load.md`
분석: `docs/analysis/pumpitea-loading-segment-flip.md`

## 한 일

1. **남은 트랩 확정.** 마운트 캐시의 실행 파일
   (`build/runtime_mounts/pumpitea/PIU/PIU.EXE`)을 `repiu_aot_probe --dump`로
   떠서 memcpy 복원 `0xFE38A`가 `pop es`(guarded pop), INT8 ISR 진입 헬퍼
   `0xFE2F0`이 `66 2E 8E 1D disp32`(0x66+CS 접두어의 메모리-소스 DS 로드,
   슬롯 없음), ISR 말미 `0x2AB80~`이 `pop gs/fs/es/ds; popad; iretd`임을
   확인했다. 분석 문서의 "mov es,[saved]" 추정을 교정했다.
2. **i386 guarded pop 슬롯**(`EmitGuardedSegmentPopSlot`)을 물리 비교 없는
   66바이트 shadow·수용 쌍 형태로 교체. `AotGuardedSegmentPopSite`에
   `pair0/pair1_address_offset`·`shadow_store_offset` 추가(0 = long-mode 옛
   레이아웃), 패처·엔진 동적 추가 오프셋·guard-fault 창을 맞춤.
3. **메모리-소스 guarded load.** `ReadGuardedSegmentLoadRegisters`가 32-bit
   주소·접두어 0x66/0x2E만·소스 세그먼트 CS/DS/SS인 `mov Sreg, r/m16`을
   `gpr_register = kAotSegmentLoadMemorySource`로 받는다. 이미터·커버리지
   검증기가 공유하는 `EncodeGuardedSegmentLoadSource`가 ModRM/SIB/disp를 ax
   목적지로 재부호화하고 ESP 베이스는 +8(disp32)로 다시 쓴다. long-mode는
   `gpr_register > 7` 거부로 INT3 boundary 유지.
4. probe: pop 레이아웃 단언 갱신, `guarded_segment_load_memory_forms`(CS+0x66
   abs32, ESP disp8→disp32 +8, [esp]→disp32 8, [ebp-4], [eax], SIB 스케일)
   추가, 16-bit 주소 형태는 HLE 잔류 확인.
5. `ARCHITECTURE.md`에 1·2단계의 i386 shadow 진실원 슬롯 절 추가.

## 검증

* `repiu_aot_probe --selector-guard` 전 항목 true, `--segment-restore` 통과.
* pumpitea 90초, 같은 디렉터리의 exe 사본 교대(기준선 = 1단계 HEAD 612251c
  Release 빌드 `repiu_base.exe`), 2026-10-07 00:50~01:12:

| 순서 | 빌드 | 로고 뒤 공백 | handled segment load | guarded pop 성공/폴백 | guarded load 성공/폴백 | 재패치 지점 기록(native/HLE) |
|---|---|---|---|---|---|---|
| 워밍업 | 2단계 | 2.23초 | 25,888 | 156,633 / **3** | 97,723 / 2,530 | 862 / 8,773 |
| 1 | 기준선 | 3.51초 | 94,388 | 92,999 / 68,462 | 100,006 / 2,525 | 1.81M / 33.87M |
| 2 | 2단계 | **2.37초** | 25,955 | 137,247 / **3** | 78,705 / 2,530 | 912 / 9,912 |
| 3 | 기준선 | 2.99초 | 71,821 | 61,909 / 45,891 | 52,678 / 2,525 | 1.81M / 33.87M |
| 4 | 2단계 | **2.09초** | 25,996 | 218,085 / **3** | 158,910 / 2,530 | 970 / 11,703 |

* guarded pop 폴백이 수만 건에서 3건으로, handled segment load가 틱·memcpy에
  비례하던 72k~94k에서 26k(런 간 ±0.4%)로, 전량 재패치의 지점 기록 합이
  35.7M에서 ~1만으로 줄었다. 완료 기준(틱·memcpy 비례 소멸) 충족.
* 로고 뒤 공백은 기준선 2.99~3.51초 → 2.09~2.37초. 오늘 기계는 어제
  저녁(1단계 6.8~7.6초)보다 전체적으로 빠른 영역에 있었고, 교대 측정 안에서는
  일관되게 수정 빌드가 짧다. v0.0.180의 5.0초보다 이미 짧다.
* 두 번째 정지(41~43초, 1.8~4.4초)가 **양쪽 빌드에 모두** 있다(기준선 1에서
  2.94초). 이번 변경과 무관한 기존 현상으로 분석 문서 미확정 항목에 기록.
* 남은 guarded load 폴백 2,530은 런 간 결정적이며 `0xFCF6F`(far 문자열 비교
  관용구) 그룹이다.
* 기동 스모크 30초: pumpit1·pumpit2a·pumpit3a 모두 정상 종료, 게스트 예외 0.
  1단계에서 4회 중 1회 보였던 pumpit3a 간헐 AV는 이번 1회에서 재현되지 않았다
  (회수가 적어 판정은 보류).
* 빌드 경고 C4819(`aot_translation_plan.h` 1행)는 HEAD에도 있는 기존 경고.

---

# Work log: shadow-authoritative pop and memory-source load slots (issue #18, direction 3 phase 2)

Settled the remaining sites by dumping the mounted executable: the memcpy
restore `0xFE38A` is `pop es`, the INT8 ISR entry helper `0xFE2F0` is
`66 2E 8E 1D disp32` (a memory-source DS load no slot accepted) and the
ISR epilogue is `pop gs/fs/es/ds; popad; iretd`. Re-emitted the i386
guarded pop slot as a 66-byte shadow/accepted-pair form (new site offsets,
patcher, dynamic-append and guard-fault window updates); taught the
classifier to accept `mov Sreg, r/m16` memory sources (32-bit addressing,
prefixes 0x66/0x2E only, CS/DS/SS source) as `kGuardedSegmentLoad` with
`gpr_register = kAotSegmentLoadMemorySource`, re-encoded by the shared
`EncodeGuardedSegmentLoadSource` (ESP base +8 in disp32 form); updated
the probe and ARCHITECTURE.

Verification: both probes pass. Interleaved 90 s pumpitea runs against
the phase-1 HEAD build: post-logo gap 2.99/3.51 s baseline vs
**2.37/2.09 s** (warm-up 2.23 s); guarded-pop fallbacks 68,462/45,891 →
**3**; handled segment loads 94k/72k → 26k (run-invariant); whole-cache
re-patch site writes 35.7M → ~10k. The machine sat in a faster regime
today than during phase 1 (6.8–7.6 s), but the interleave is consistent
and the gap is already below v0.0.180's 5.0 s. A second 1.8–4.4 s stall
at 41–43 s appears in both builds and is unrelated. pumpit1/pumpit2a/
pumpit3a 30 s smokes clean (one run each; the phase-1 intermittent
pumpit3a AV did not recur). The C4819 warning on
`aot_translation_plan.h` predates this change.
