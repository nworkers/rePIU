# 작업 로그: AOT `other` 경계 규명(a) + 체류량(b)
# Work Log: Characterize `other` boundaries (a) + Residency (b)

**Task:** 263 (Task 262 후속 a·b)
**브랜치 (Branch):** `claude/task-262-aot-dynamic-perf-aipxsa`
**설계 (Design):** `docs/design/20260722-263-aot-other-boundary-and-residency.md`
**작업 지시 (Work order):** `docs/work-orders/20260722-263-aot-other-boundary-and-residency.md`

## 한 일 (What was done)

Task 262가 남긴 두 질문에 답하는 진단 계측을 추가하고 실측했다.

* **(a)** `other`(비전달 명령) 77%의 정체를 선두 opcode 히스토그램 + 최근 표본으로 규명.
* **(b)** AOT 체류량(블록 진입당 직선 명령 수)과 커버리지 추정치를 Zydis 디코드로 산출.

### 변경 파일 (Changed files)

* `thread_context.h` — (a) `aot_other_opcode_histogram[256]`, last-other eip/bytes;
  (b) residency total/samples/max.
* `aot_runtime_dispatch.{h,cpp}` — `RecordAotOtherBoundarySample`(a),
  `AccumulateAotResidency`(b, Zydis 직선 디코드 상한 64). residency 훅은 게스트
  진입 EIP가 명확한 캐시 진입 4지점(indirect·conditional·return·retired 재해결·
  single-step 재해결).
* `live_telemetry.h` — (a) top-opcode/last-other 4필드, (b) residency 3필드,
  `kWin32LiveTelemetryVersion` 17 → 18.
* `execution_trampoline.h` / `live_telemetry_snapshot.cpp` — top-8 히스토그램 계산·복사.
* `host/win32/main.cpp` — 정상 종료 요약에 top-8 opcode + residency/coverage.
* `supervisor_main.cpp` — 주기 덤프에 top opcode·last-other·residency.

## 검증 (Verification)

* **VS 2026 Debug 빌드 통과**(exit 0), loader/supervisor 재링크.
* **실측 3회.** supervisor aot-dynamic 120초, loader 단독 aot-dynamic 45초(graceful
  summary top-8용). 불변식·필드 정상.

## 실측 결과 (Measurement results)

### (a) `other` = 세그먼트 명령 (확인됨)

loader 단독 45초 graceful summary, `other` 총 46,365의 top-8:

| opcode | 명령 | 카운트 | other 대비 |
|---|---|---:|---:|
| `0x65` | GS: 세그먼트 프리픽스 | 25,564 | 55.1% |
| `0x55` | push ebp (프롤로그) | 4,088 | 8.8% |
| `0x8E` | mov Sreg, r/m | 3,554 | 7.7% |
| `0x1E` | push ds | 3,215 | 6.9% |
| `0x8C` | mov r/m, Sreg | 2,058 | 4.4% |
| `0x07` | pop es | 1,915 | 4.1% |
| `0x06` | push es | 1,894 | 4.1% |
| `0x1F` | pop ds | 1,576 | 3.4% |

top-8이 `other`의 94.6%. 세그먼트 명령(0x55 제외)이 `other`의 약 86% = **전체 AOT
이탈의 약 75%**(39,776 / 52,778). 게임이 세그먼트-집약적이라 AOT가 세그먼트 오버라이드
메모리 접근·세그먼트 레지스터 이동·세그먼트 push/pop을 번역 못 하고 각각 sentinel →
단일스텝. 잔여 0x55(push ebp)는 함수 프롤로그 커버리지 공백(마지막 표본 0x030F5211
바이트 `51 56 57 55` = push ecx/esi/edi/ebp가 뒷받침).

### (b) 체류량 (확인됨, 프록시)

평균 **4.82명령/진입**(254,527 / 52,813), max 64, 커버리지 추정 **42.5%(상한)**.
프록시가 세그먼트 mid-block sentinel을 지나쳐 세므로 과대평가 — 실제 커버리지는 더
낮다. 단일스텝이 dispatch의 98.6%라는 사실과 정합.

### 처리량 (supervisor 120초)

legacy progress 606,613 대 aot-dynamic 29,457(20.6x). aot-dynamic 경계 78,701 중
other 59,208. (간접 분기 indir는 45초 단독에서 44, 120초에서 12,302 — ≈77초 이후
프레임 루프의 후반 현상, Task 262와 일치.)

## 결론 (Conclusion)

aot-dynamic이 느린 근인은 인라인 캐시 churn(간접 분기)이 아니라 **세그먼트 명령을
번역하지 못해 대량 단일스텝**하기 때문이다. Task 204·262의 간접 분기 가설이 반증됐다.
개선 방향(미착수): (1) AOT 변환기가 흔한 세그먼트 오버라이드 메모리 접근(특히 GS)과
세그먼트 레지스터 이동을 selector-shadow 기반으로 번역, (2) 함수 진입 커버리지 공백
축소. 상세는 `docs/analysis/current-execution-frontier.md` Task 263.

## English summary

Instrumented the `other` boundary bucket's lead-opcode histogram and an AOT
residency proxy, then measured. (a) The `other` 77% bucket is dominated by
segmentation instructions: of 46,365 `other`, the top-8 (94.6% coverage) are GS
prefix 0x65 (55%), push ebp 0x55 (9%), mov Sreg 0x8E (8%), push ds 0x1E (7%),
mov-from-Sreg 0x8C (4%), pop es 0x07 (4%), push es 0x06 (4%), pop ds 0x1F (3%) —
segment ops (all but push ebp) are ~86% of `other`, i.e. ~75% of ALL boundary
exits. The AOT translator cannot translate segment-override memory access,
segment-register moves, or segment push/pop, so it sentinels each and single-steps
it through the HLE selector-shadow path. (b) Residency proxy: 4.82 instr/entry
avg, coverage ≤42.5% (upper bound; segment-op sentinels are mid-block and the
proxy counts past them). Root cause pinned; both the inline-cache-churn and
indirect-branch hypotheses are disproved. Verified via a VS 2026 Debug build and
three supervised/standalone runs.
