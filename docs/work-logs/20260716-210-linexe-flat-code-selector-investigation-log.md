# 작업 로그: 0x030F3438 폭풍 조사 및 해소 (Task 210)

* 작업 지시: `docs/work-orders/20260715-210-linexe-flat-code-selector-investigation.md`
* 브랜치: `feature/210-linexe-flat-code-selector-investigation`

## 1. 결론 요약

`0x030F3438` 폭풍의 원인은 작업 지시의 유력 가설(LINEXE 별도 selector 설계로 thunk `cmp dx, cx` assertion이 구조적으로 실패)이 **아니었다**. 실제 원인은 Task 208 머지의 물리 우선 `ReadGuestSegmentSelector`가 **호스트 LDT에 존재할 수 없는 합성 selector(shadow GS=`0x0020`, DOS/4G client-data selector)를 가린 것**이며, 이로 인해 DLL loader 초기화의 GS 검사가 실패해 공용 fatal-tail `0x030F3438`로 빠졌다. 읽기 규칙 수정으로 두 백엔드의 증상(aot-dynamic 재시도 폭풍, trap 7.2초 fatal 종료)이 모두 해소되었다.

## 2. 진단 과정

1. **trap 백엔드 fatal 실측:** trap 백엔드는 약 7.2초에 게스트가 `INT 21h AX=4C01h`로 자체 종료하는데(직전 관측), 정상 종료라서 로더 전체 요약이 남는다. 요약의 fatal 기록이 결정적 증거였다:
   * `handled original fatal breakpoint count: 1`, 주소 `0x030F3438`
   * 메시지 `0x031A623C` = **"Fatal error: unable to initialize DLL loader."**
   * 즉 발화한 검사는 Task 209가 역어셈블한 thunk 패처(`EDX=0x011A628D` 계열)가 아니라 **DLL loader 초기화 검사**다. `0x030F3438`은 여러 `jnz` 실패 경로가 공유하는 fatal-tail이다(2026-07-11 분석과 일치).
2. **기제 확정 (코드):** `INT 21h AX=FF00h` HLE는 `context->guest_gs = kDos4gwClientDataSelector(0x0020)`로 **shadow만** 갱신한다. `0x0020`은 `RegisterDescriptor`로 소프트웨어 SelectorTable에만 등록되며 호스트 LDT 엔트리가 없어(NtSetLdtEntries 미사용) 물리 레지스터에 실을 수 없다. 물리 우선 읽기는 물리 GS(호스트 진입값 `0x2B`)를 반환하므로 이 selector는 게스트에게 영원히 보이지 않게 된다.
3. **99f60de 기준선과의 정합:** 머지 전(shadow 우선)에는 fatal 카운트 0으로 DLL loader가 정상 초기화되어 Glide 창 생성까지 진행했다 — 물리 우선 도입 시점과 증상 발생이 일치한다.

## 3. 수정 내용

`ReadGuestSegmentSelector`(execution_trampoline.cpp)에 다음 규칙을 추가했다.

* 물리 우선은 유지한다 (Task 208이 고친 문제 — 하드웨어가 직접 로드한 selector를 shadow가 못 따라가는 것 — 보존).
* 단, **물리 값이 호스트 진입 시점 selector(`g_recovery_host_es/ds/fs/gs`)와 같고, shadow selector가 SelectorTable에 등록되어 present이면 shadow를 반환**한다. 물리 레지스터가 호스트 진입값 그대로라는 것은 게스트가 그 레지스터를 하드웨어로 로드한 적이 없다는 뜻이므로, HLE가 에뮬레이트한 shadow가 권위 있는 값이다.
* SS는 호스트 진입값을 기록하지 않으므로 기존 물리 우선을 유지한다.

진단 계측: 공유 텔레메트리 version 11에 `fatal_breakpoint_count`/`fatal_message_address`(fatal 발생 즉시 미러)와 `seg_divergence_count`/`seg_divergence_reg_physical`/`seg_divergence_shadow`(물리·shadow 불일치 기록)를 추가하고 supervisor 스냅샷 출력을 확장했다.

## 4. 검증

빌드: `build\win32_x86_debug` 대상 빌드 통과.

| 구동 | 수정 전 (v0.0.42 main) | 수정 후 |
| --- | --- | --- |
| aot-dynamic 30초 | `0x030F3438` 폭풍, `progress=0`, 약 137k dispatch/s | 폭풍 소멸: `progress=8,449`, Glide ordinal `0x5E`, **MSCDEX `request/cmd/status=1/3/0x100`** (실험 패치 없이 처리), fatal_count=0 |
| trap 30초 | 7.2초 `AX=4C01h` fatal 종료 (dispatch 781,653) | fatal 소멸: 30초 완주, `progress=641,013`, 디코드 구간 `0x030873CD` 도달, fatal_count=0 |
| aot-dynamic 180초 | — | 99f60de 기준선 복원 확인: 창 1회(640x480), MSCDEX request 1건 처리(ES=`0x100`, 거절 0), 게스트는 기존 frontier인 guest `0x030873F4` 디코드 스토어로 종료(exit 2), 이후 알려진 로더 hang → supervisor 마감 회수 |

물리/shadow 불일치는 aot 30초에서 41,989건, trap 30초에서 206,237건 계측되었다(마지막 기록: DS, 물리 `0x2B`/shadow `0x0000` — shadow 미설정 레지스터는 물리가 승리하는 정상 경로).

## 5. 판정과 남은 항목

* 작업 지시의 항목 1(`0x010F3648` resolver 정적 분석)과 2(flat selector 모델 검증)는 **폭풍 해소에 불필요**해졌다. LINEXE selector 설계가 실기 DOS4GW와 같은지는 비차단 별도 주제로 남는다.
* Task 209 분석 문서에 정정 섹션을 추가했고, frontier 문서에 Task 210 항목을 기록했다.
* 실행 frontier는 Task 205~206의 디코드 루프 미매핑 스토어(`0x030873F4` → `0x045D3EB0`)로 복귀했다. 로더 post-attempt hang(Task 204 발견)도 여전히 남아 있다.

---

# Work Log: Investigating and Resolving the 0x030F3438 Storm (Task 210)

**Conclusion.** The storm was not caused by the work order's leading hypothesis (per-module LINEXE selectors structurally failing the thunk `cmp dx, cx` assertion). The real cause: Task 208's physical-first `ReadGuestSegmentSelector` hid a **synthetic selector that can never exist in the host LDT** — shadow GS=`0x0020`, the DOS/4G client-data selector installed by the `INT 21h AX=FF00h` HLE — so the DLL loader's initialization GS check failed into the shared fatal-tail at `0x030F3438`. The decisive evidence was the trap backend's clean-exit summary: one fatal breakpoint at `0x030F3438` with message `"Fatal error: unable to initialize DLL loader."` (`0x031A623C`), proving the firing check was the DLL-loader one, not the thunk patcher; the trap backend's ~7.2 s `AX=4C01h` self-termination and the aot-dynamic retry storm were downstream behaviors of the same failure.

**Fix.** Keep physical-first, but return the shadow selector when the physical register still equals the host entry-time selector (`g_recovery_host_*`) and the shadow is registered and present in the SelectorTable — a physical register still at its host entry value means the guest never hardware-loaded it, so the HLE shadow is authoritative. Diagnostics added: telemetry version 11 with fatal count/message mirroring and physical/shadow divergence counters, plus supervisor snapshot output.

**Verification.** Full target build passes. aot-dynamic 30 s: storm gone (progress 0 → 8,449, Glide ordinal `0x5E`, MSCDEX `request/cmd/status=1/3/0x100` handled on main without any experiment patch, zero fatals). Trap 30 s: no more 7.2 s fatal (full 30 s, progress 641,013, reaching the decode region). aot-dynamic 180 s: restores the 99f60de baseline exactly — one 640x480 window, one handled MSCDEX request (ES=`0x100`, zero declines), guest death at the known decode-loop store guest `0x030873F4` (exit code 2), followed by the known post-attempt loader hang. The execution frontier returns to the Task 205–206 unmapped decode-loop store; the work order's static resolver analysis and flat-selector unification items are no longer needed to resolve the storm and remain as a separate non-blocking topic.
