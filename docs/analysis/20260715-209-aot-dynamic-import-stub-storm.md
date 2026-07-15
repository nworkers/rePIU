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

### 5) `0x030F3438`은 사고가 아니라 DOS4GW 런타임 자체의 의도된 assertion 트랩입니다
`0x010F3400` 부근을 역어셈블하면 (aot_probe 기준 주소, 실제로는 `+0x02000000`):
```
0x10F3413  call 0x010F3648        ; 세그먼트 해석 서브루틴 호출
0x10F3418  add esp, 0x0C
0x10F341B  pop eax                ; eax = 패치할 목표 함수 오프셋
0x10F341C  pop edx                ; edx = 목표 함수의 (기대) 코드 세그먼트
0x10F341D  mov cx, cs             ; cx = 현재 실제 실행 중인 코드 세그먼트
0x10F3420  cmp dx, cx
0x10F3423  mov edx, 0x11A628D
0x10F3428  jnz 0x010F3438         ; cx != dx 이면 트랩으로 직행
0x10F342A  sub eax, edi
0x10F342C  mov byte ptr [edi], 0xE9   ; (일치 시) JMP rel32 thunk를 자체 패치
0x10F342F  sub eax, 0x05
0x10F3432  mov [edi+0x01], eax
0x10F3435  popad
0x10F3436  jmp eax                ; 패치된 thunk로 점프
0x10F3438  int3                   ; <- cx != dx 일 때 항상 여기로
0x10F3439  push edx
0x10F343A  call 0x010F42E8        ; fatal 메시지 출력 루틴(추정)
0x10F343F  hlt
```
이 `int3 52 E8 xx xx xx xx F4` (`CC 52 E8 .. F4`) 패턴은 `HandleOriginalFatalBreakpoint`
(`execution_trampoline.cpp:7390`)가 **이미 인식하는** DOS4GW 런타임의 "fatal breakpoint" 관용구입니다
— `EDX`가 가리키는 ASCIZ 메시지를 읽고 EIP를 1바이트 전진시켜 계속 진행시킵니다. 즉 이 트랩 자체는
설계상 "한 번 찍고 지나가는" 것이지 무한 루프를 의도한 게 아닙니다. 그런데도 우리 실행에서는 30초 동안
동일 EIP에서 85만~128만 회 예외가 발생합니다 — 이는 EIP가 그 자리에 영원히 박혀있다기보다,
**호출자가 이 실패를 계속 재시도하며 매번 같은 조건(`cx != dx`)으로 같은 트랩에 도달**하고 있음을
시사합니다(진단 progress count가 시종 0인 것과 일치).

`cx`(`mov cx, cs`)는 현재 실제로 실행 중인 코드 세그먼트이고, `dx`는 스택 인자로 전달된 "패치 대상
함수가 속한 세그먼트"입니다. 우리 로더 로그의 `Win32 relocated selector binding`을 보면 PIU.EXE 자체
LE 오브젝트들은 `0x001C/0x0024/0x002C/0x0034` selector를 쓰고, 반면 LINEXE_LOADER로 추출된 내부
모듈은 `kDos4gwLinexeCodeSelector = 0x0080`(및 `0x0088`, `0x0090`)이라는 **서로 다른 별도의 고정
selector**를 씁니다. 실기 DOS4GW라면 이런 "논리적 모듈"들도 결국 하나의 flat 코드 세그먼트를 공유할
가능성이 높은데, 우리 구현은 LINEXE_LOADER를 별도 selector로 등록하고 있어 `cx == dx` 비교가
PIU.EXE 코드 → LINEXE_LOADER 함수로의 (정상적일) cross-segment 패치 시도에서 구조적으로 항상
실패할 수 있습니다. 이것이 사실이라면, 근본 수정은 세그먼트 shadow/물리 동기화가 아니라 **LINEXE 코드
selector를 PIU.EXE 본체와 같은 flat CS로 통합하거나, 이 리졸버가 cross-segment 케이스에서 올바르게
동작하도록 재현**하는 것입니다. 이 가설은 아직 확정이 아니며, `0x010F3648`이 정확히 무엇을 위한
서브루틴인지(범용 cross-segment thunk resolver인지, LINEXE 전용인지) 추가 검증이 필요합니다.

## 결론 (Conclusion)

1. `ReadGuestSegmentSelector`가 물리 `CONTEXT` 세그먼트 레지스터를 우선 읽도록 한 수정과
   `RecordGuestSegmentLoad`(`MOV Sreg,r/m`)의 물리 레지스터 write-through 추가는 **기본 trap
   백엔드에서 회귀를 일으키지 않는 정당한 개선**입니다(`MOV ES/DS/FS/GS`가 shadow만 갱신하고 물리
   레지스터는 갱신하지 않던 기존 결함을 수정).
2. `0x030F3438`은 AOT 변환기가 계산을 잘못해서 우연히 도달하는 주소가 아니라, DOS4GW 런타임 코드에
   원래부터 존재하는 **의도된 assertion 트랩**(`cmp dx, cx; jnz`)입니다. 이 트랩은
   `HandleOriginalFatalBreakpoint`가 이미 인식하여 EIP를 1바이트 전진시키므로 그 자체로는 무한 루프가
   아니지만, 호출자가 매번 같은 실패 조건으로 재시도하면서 사실상 무한 재시도 루프가 됩니다. AOT
   트랜스퍼 핸들러가 `0xCC`를 인식 못 해 멈춘다는 이전 서술은 부정확했습니다 —
   `HandleOriginalFatalBreakpoint`가 정확히 이 패턴을 처리하도록 이미 구현돼 있었습니다.
3. 유력한 가설은 **selector 불일치**입니다: 이 트랩은 "패치 대상 함수의 세그먼트(`dx`)가 현재 실행 중인
   코드 세그먼트(`cx = cs`)와 같은가"를 검사합니다. 우리 구현은 LINEXE_LOADER 내부 모듈에
   `kDos4gwLinexeCodeSelector = 0x0080`(및 `0x0088`, `0x0090`)이라는, PIU.EXE 본체의 LE 오브젝트
   selector(`0x001C/0x0024/0x002C/0x0034`)와는 별개인 고정 selector를 부여합니다. 실제 DOS4GW라면
   이런 논리적 모듈들도 하나의 flat 코드 세그먼트를 공유했을 가능성이 있고, 그렇다면 우리 구현의
   "LINEXE 모듈마다 별도 selector" 설계 자체가 이 assertion을 구조적으로 항상 실패시키는 원인일 수
   있습니다. 이 가설은 `0x010F3648`(해석 서브루틴)이 정확히 무엇을 하는지, 그리고 실기 DOS4GW에서
   `dx`/`cx`가 실제로 같은 값이었는지를 추가로 검증해야 확정됩니다.

## 미확정 (Open)

* `0x010F3648` 서브루틴의 역할 확정: 범용 cross-segment thunk resolver인지, 특정 LINEXE 케이스
  전용인지, 그리고 `dx`(대상 세그먼트)가 정확히 어디서 오는지
* LINEXE 내부 모듈에 별도 selector를 부여하는 현재 설계가 실기 DOS4GW의 flat 세그먼트 모델과
  실제로 다른지 확인 — 다르다면 selector 통합이 근본 수정
* `aot-dynamic`에서만 재현되고 trap 백엔드에서는 (같은 트랩에 걸리더라도) 왜 계속 진행되는지
  (재시도 빈도나 호출자 로직의 백엔드별 차이 여부)
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
4. **`0x030F3438` is not a stray landing spot — it's DOS4GW's own intentional assertion trap.**
   Disassembling around `0x010F3400` (aot_probe addressing; add `+0x02000000` for the real runtime base)
   shows:
   ```
   0x10F3413  call 0x010F3648        ; segment-resolution subroutine
   0x10F341B  pop eax                ; eax = patch target offset
   0x10F341C  pop edx                ; edx = target function's (expected) code segment
   0x10F341D  mov cx, cs             ; cx = the segment actually executing right now
   0x10F3420  cmp dx, cx
   0x10F3428  jnz 0x010F3438         ; mismatch -> straight to the trap
   0x10F342C  mov byte ptr [edi], 0xE9   ; (match) self-patch an E9 JMP rel32 thunk
   0x10F3436  jmp eax                ; jump through the freshly patched thunk
   0x10F3438  int3                   ; <- always lands here when cx != dx
   0x10F3439  push edx
   0x10F343A  call 0x010F42E8        ; presumed fatal-message printer
   0x10F343F  hlt
   ```
   This `int3 52 E8 .. F4` byte pattern is exactly what `HandleOriginalFatalBreakpoint`
   (`execution_trampoline.cpp:7390`) already recognizes as DOS4GW's "fatal breakpoint" idiom: it reads the
   ASCIZ message pointed to by EDX and advances EIP by one byte to keep going. So this trap is designed to
   fire-and-continue once, not to hang. The 30-second storm of 850k–1.28M dispatches at this exact address
   (with `diagnostic progress count` staying at 0 throughout) is better explained as **the caller retrying
   the same failing resolution over and over**, hitting the identical `cx != dx` condition every time — not
   an AOT transfer-handler bug.

## Conclusion
1. The `ReadGuestSegmentSelector` physical-register-preference fix and the `RecordGuestSegmentLoad`
   physical write-through are legitimate corrections with no regression under the default trap backend
   (they fix a real gap: `MOV ES/DS/FS/GS` previously updated only the shadow register, never the
   physical one).
2. `0x030F3438` is a pre-existing, intentional assertion trap in the DOS4GW runtime's cross-segment call
   thunk patcher, already handled once by `HandleOriginalFatalBreakpoint` — not an AOT translation bug.
   The storm is the *caller* retrying the same failing check indefinitely, not EIP getting stuck.
3. Leading hypothesis: **selector mismatch by design.** The trap checks whether the target function's
   segment (`dx`) equals the segment currently executing (`cx = cs`). Our implementation assigns the
   LINEXE_LOADER internal module its own fixed selectors (`kDos4gwLinexeCodeSelector = 0x0080`, plus
   `0x0088`/`0x0090`), distinct from PIU.EXE's own LE object selectors (`0x001C/0x0024/0x002C/0x0034`). If
   real DOS4GW instead shares one flat code segment across these logical modules, our "separate selector
   per LINEXE module" design would make this assertion fail structurally, every time. This needs
   confirming: what `0x010F3648` actually computes for `dx`, and whether real DOS4GW keeps `cs` constant
   across a PIU.EXE-to-LINEXE_LOADER call.

## Open
* Pin down what `0x010F3648` computes and where `dx` (the expected target segment) ultimately comes from.
* Determine whether assigning LINEXE_LOADER its own selectors (vs. reusing PIU.EXE's flat CS) is the actual
  design mismatch; if so, selector unification is the real fix.
* Why the trap-mode backend keeps making progress despite hitting the same trap, while `aot-dynamic` stalls
  entirely — possibly a difference in retry cadence or caller-side logic between backends.
* Whether `LINEXE bridge entry`/`Glide gate entries` staying at 0 for 30s under the trap backend is simply
  insufficient time, or a separate defect.
