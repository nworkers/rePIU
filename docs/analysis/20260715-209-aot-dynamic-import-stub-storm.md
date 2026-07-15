# LINEXE 무한 루프 재조사: aot-dynamic 백엔드의 import-stub 예외 폭풍
# Re-investigation of the LINEXE Stall: an aot-dynamic Import-Stub Exception Storm

## 배경 (Background)

`analysis_results.md`/`implementation_plan.md` (Task 208)는 LINEXE 모듈 스캔 루프가 진행되지 않는
원인을 "POP ES/FS/GS 하드웨어 패스스루로 인한 shadow segment 불일치"로 지목하고,
`ReadGuestSegmentSelector`가 물리 `CONTEXT` 세그먼트 레지스터를 우선 읽도록 수정하는 방안을
제시했습니다. 이 문서는 그 수정을 실제로 적용하고 검증한 결과와, 검증 과정에서 드러난
**실제 근본 원인**(세그먼트 shadow 문제가 아님)을 기록합니다.

## 확인됨 (Confirmed)

### 1) 진단 카운터는 실행 백엔드에 종속적입니다
`module candidate/match`, `LINEXE scan entry/match/return`, `export name compare` 등 대부분의
LINEXE 진단 카운터는 `HandleSingleStepTrace` 내부에서만 갱신되며, 이는
`enable_single_step_trace`가 켜진 실행 경로(`AttemptWin32GuestStackTrapExecution`, 기본 백엔드)
에서만 동작합니다. `REPIU_EXECUTION_BACKEND=aot-dynamic`(`AttemptWin32GuestStackAotExecution`)는
이 플래그를 항상 `false`로 넘기므로, 이 백엔드로는 해당 카운터들이 **버그 유무와 무관하게 항상 0**으로
찍힙니다. (`LINEXE bridge entry`는 예외입니다 — `HandleLinexeFarTransferBoundary`가 백엔드와 무관하게
호출되므로 유효한 신호입니다.)

이 사실을 모른 채 `aot-dynamic` 백엔드로만 검증하면 "module candidate/match: 0/0"을 실패로 오판하게
됩니다. 반드시 기본(`trap`) 백엔드로 교차 검증해야 합니다.

### 2) 기본(trap) 백엔드에서는 세그먼트 수정 전후 모두 정상 진행합니다
동일한 30초 실행을 `REPIU_EXECUTION_BACKEND` 미설정(기본 trap 백엔드) 상태로 비교하면:

| 상태 | diagnostic progress count | module candidate/match | 비고 |
|---|---|---|---|
| 수정 전 (`99f60de`) | 111,377 | 1/1 | 정상 진행, 무한루프 없음 |
| 수정 후 (본 세션 fix) | 112,072 | 0/0 | 정상 진행, 무한루프 없음 |

두 상태 모두 30초간 10만 회 이상 진행하며 멈추지 않습니다. `LINEXE bridge entry`/`Glide gate entries`는
둘 다 0인데, 이는 30초 안에 그 지점까지 도달하지 못했을 뿐일 가능성이 높습니다(무한루프가 아님).

### 3) `aot-dynamic` 백엔드에서는 세그먼트 수정 전후 모두 30초 안에 막힙니다
| 상태 | 결과 |
|---|---|
| 수정 전 (`99f60de`) | `progress=14`에서 native 실행 중 ACCESS_VIOLATION으로 크래시 |
| 수정 전 (`aebbbb6`, POP ES/FS/GS 미개입) | 동일하게 `progress=14`, 동일 EIP(`0x030F6574`)에서 크래시 — **POP ES/FS/GS 개입 여부는 이 크래시에 영향 없음** |
| 수정 후 (`ReadGuestSegmentSelector` 물리 레지스터 우선 + `RecordGuestSegmentLoad` write-through) | `progress=0`, `0x030F3438`에서 EIP가 전혀 전진하지 못하는 예외 폭풍(30초간 dispatch 858k~128만 회) |

`aebbbb6`(POP ES/FS/GS 완전 패스스루, 작은 selector limit)와 `99f60de`(POP ES/FS/GS HLE 개입,
동일한 작은 limit)가 **완전히 동일한 크래시**를 재현한다는 것은, POP GS(selector `0x0090`)가 유효한
LDT 항목이라 애초에 하드웨어가 직접 처리하며 VEH를 전혀 거치지 않기 때문입니다. 즉
`docs/work-logs/20260715-206-exception-diagnostic-and-buffer-provenance-log.md`가 "POP ES/FS/GS 개입
제거로 `0x030F3438` 폭풍을 해소했다"고 기록한 결론은 이번 재현에서 성립하지 않았습니다 — 같은 코드로
다시 실행하면 `aot-dynamic`에서 크래시가 재현됩니다. 세그먼트 POP 개입 여부는 이 증상과 무관한
요인이었을 가능성이 높습니다(비결정적 타이밍이었거나, 당시 다른 요인이 함께 바뀌었을 수 있습니다).

### 4) `0x030F3438` 폭풍의 직접 메커니즘
`0x030F3438`을 역어셈블하면:
```
... 89 47 01   mov [edi+1], eax   ; 해석된 함수 주소 저장(추정)
    61         popad
    FF E0      jmp eax            ; 간접 점프 — EAX가 실행 대상
   [CC]        int3               ; EAX가 이 패딩/트랩 바이트로 계산됨
    52         push edx
    E8 ...     call ...
    F4         hlt
```
`0xCC`는 원본 `PIU.EXE`에 실제로 존재하는 바이트입니다(정적 분석과 런타임 크래시 덤프 양쪽에서 동일하게
확인). `HandleAotIndirectTransfer`/`HandleAotReturnTransfer`/`HandleAotConditionalTransfer`는
`E8/E9/EB/FF/Jcc` opcode만 인식하므로, 일단 `0xCC`(비인식 opcode)에 도달하면 아무 핸들러도 EIP를
전진시키지 못하고 TF 플래그만 세운 채 같은 주소를 무한 재실행합니다.

## 결론 (Conclusion)

1. `ReadGuestSegmentSelector`가 물리 `CONTEXT` 세그먼트 레지스터를 우선 읽도록 한 수정과
   `RecordGuestSegmentLoad`(`MOV Sreg,r/m`)의 물리 레지스터 write-through 추가는 **기본 trap
   백엔드에서 회귀를 일으키지 않는 정당한 개선**입니다(`MOV ES/DS/FS/GS`가 shadow만 갱신하고 물리
   레지스터는 갱신하지 않던 기존 결함을 수정).
2. 그러나 `analysis_results.md`가 지목한 "LINEXE 무한 루프"의 실제 원인은 세그먼트 shadow/물리
   불일치가 아니라, **`aot-dynamic` 백엔드의 동적 이진 변환 엔진 자체의 결함**입니다: 어떤 경로에서
   `jmp eax`의 EAX가 결정론적으로 잘못된 주소(`0x030F3438`, 코드가 아닌 패딩 위치)로 계산되고, AOT
   트랜스퍼 핸들러가 그 지점의 opcode(`0xCC`)를 인식하지 못해 영원히 멈춥니다.
3. 이 EAX 오염이 `aot-dynamic` 고유의 문제(예: 간접 호출 대상 캐싱, inline cache patch, 또는 AOT
   변환된 코드에서 레지스터 상태가 원본과 달라지는 문제)인지, 아니면 LINEXE export 해석 자체가
   여전히 실패해서(원본 이슈) EAX가 오염되는지는 **추가 조사가 필요**합니다. trap 백엔드에서
   `module candidate/match`가 수정 전 `1/1` → 수정 후 `0/0`으로 바뀐 점은 세그먼트 수정이 export
   매칭 타이밍에 실제 영향을 준다는 신호이므로, 다음 조사는 이 지점(EAX가 설정되는 지점)의 역추적으로
   이어갑니다.

## 미확정 (Open)

* `jmp eax`의 EAX 값이 설정되는 지점(LINEXE export 해석 트램폴린 추정)까지 역추적
* `aot-dynamic`에서만 재현되고 trap 백엔드에서는 재현되지 않는 이유(같은 게스트 코드, 다른 실행 경로)
* `LINEXE bridge entry`/`Glide gate entries`가 trap 백엔드 30초 실행에서도 0인 이유 — 단순 시간
  부족인지 별도 결함인지

---

# Re-investigation of the LINEXE Stall: an aot-dynamic Import-Stub Exception Storm (EN)

## Background
The prior analysis (`analysis_results.md`/`implementation_plan.md`, Task 208) attributed the LINEXE
module-scan stall to shadow/physical segment register desync caused by POP ES/FS/GS executing natively
(hardware pass-through) without updating rePIU's shadow tracking, and proposed making
`ReadGuestSegmentSelector` prefer the live physical `CONTEXT` segment registers. This document records
what happened when that fix was implemented and verified, and the actual root cause the verification
surfaced (not a segment shadow problem).

## Confirmed
1. **Most LINEXE diagnostic counters are backend-gated.** They only update inside
   `HandleSingleStepTrace`, which requires `enable_single_step_trace = true` — true only for the default
   trap backend, never for `aot-dynamic`. Testing exclusively with `aot-dynamic` makes "module
   candidate/match: 0/0" look like a bug regardless of whether one exists; cross-checking against the
   default backend is required. (`LINEXE bridge entry` is backend-independent since
   `HandleLinexeFarTransferBoundary` runs unconditionally.)
2. **With the default trap backend, both pre-fix and post-fix builds progress normally** (progress count
   ~111k–112k in 30s, no stall), regardless of whether the segment-selector fix is applied.
3. **With `aot-dynamic`, both pre-fix and post-fix builds fail within 30s**, but differently: pre-fix
   crashes with ACCESS_VIOLATION at `progress=14`; post-fix loops forever at `progress=0`, stuck at
   `0x030F3438`. Critically, commit `aebbbb6` (POP ES/FS/GS never intercepted) reproduces the *exact same*
   crash as `99f60de` (POP ES/FS/GS intercepted) — because POP GS with a valid LDT selector executes
   natively either way and never traps into our VEH. This means the POP ES/FS/GS interception question is
   unrelated to this failure, contradicting the resolution claimed in
   `docs/work-logs/20260715-206-exception-diagnostic-and-buffer-provenance-log.md` (not reproducible on
   retest).
4. **Mechanism of the `0x030F3438` storm:** an indirect jump (`jmp eax`, preceded by `mov [edi+1], eax`
   and `popad` — plausibly a resolved-import trampoline) lands on a literal `0xCC` byte present in the
   original `PIU.EXE` (confirmed both via static analysis of the untouched file and the runtime crash
   dump). None of the AOT transfer handlers (`HandleAotIndirectTransfer` /
   `HandleAotReturnTransfer` / `HandleAotConditionalTransfer`) recognize opcode `0xCC`, so EIP never
   advances past it, and the exception fires forever at the same address.

## Conclusion
1. The `ReadGuestSegmentSelector` physical-register-preference fix and the `RecordGuestSegmentLoad`
   physical write-through are legitimate corrections with no regression under the default trap backend
   (they fix a real gap: `MOV ES/DS/FS/GS` previously updated only the shadow register, never the
   physical one).
2. The actual root cause of the original "LINEXE infinite loop" is **not** segment shadow/physical
   desync — it is a defect in the `aot-dynamic` dynamic binary translation engine itself: some path
   deterministically computes a wrong `jmp eax` target (`0x030F3438`, a non-code padding address), and the
   AOT transfer handlers don't recognize the byte they land on, so execution never advances.
3. Whether this EAX corruption is intrinsic to `aot-dynamic` (indirect-call target caching, inline-cache
   patching, or register-state divergence in translated code) or is a downstream symptom of the LINEXE
   export resolution still failing is unresolved. The trap-backend shift from `module candidate/match: 1/1`
   (pre-fix) to `0/0` (post-fix) suggests the segment fix does change export-matching timing, so the next
   step is tracing back to where EAX is set before this jump.

## Open
* Trace back to where EAX is set before the `jmp eax` at `0x030F3438` (suspected LINEXE export-resolution
  trampoline).
* Why this reproduces only under `aot-dynamic` and not under the trap backend, given identical guest code.
* Whether `LINEXE bridge entry`/`Glide gate entries` staying at 0 for 30s under the trap backend is simply
  insufficient time, or a separate defect.
