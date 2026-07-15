# 작업 지시: MSCDEX real-mode 요청 거절 진단 및 해석 수정 (Task 211)

설계: `docs/design/20260716-211-mscdex-real-mode-request-hle.md`
브랜치: `feature/211-mscdex-real-mode-request-hle`

## 배경 요약

2026-07-16 300초 관측에서 PIU의 첫 MSCDEX `AX=1510h` 요청(DPMI `0300h` 프레임 경유)이 `HandleMscdexRequest` 초입에서 거절되어 request 카운트 0으로 남았다. 현 텔레메트리로는 거절 사유(버퍼 해석 실패 vs 헤더 길이 부족)를 구분할 수 없다.

## 재현 제약 (착수 시점 확인)

머지된 main 기준:

* aot-dynamic: `0x030F3438` assertion 폭풍으로 progress=0 정지 (90초 재현 확인, Task 210 대기).
* trap(기본): 약 7초에 게스트가 `INT 21h AX=4C01h` fatal 종료 경로로 자체 종료 (600초 의도 구동에서 확인, `exception=0x80000004`, `EAX=0x4C01`).

→ 두 백엔드 모두 MSCDEX 미도달. 진단·검증 구동은 설계의 대안 2(진단용 임시 로컬 실험: `ReadGuestSegmentSelector` 물리 우선 반환을 작업 사본에서만 비활성화, **커밋 금지**)로 수행하고 실험 사실을 작업 로그에 명시한다.

## 작업 항목

1. `[x]` 설계 문서 작성 (`docs/design/20260716-211-mscdex-real-mode-request-hle.md`)
2. `[x]` Phase A — 진단 계측
   * `[x]` `ThreadContext`: `mscdex_frame_es`, `mscdex_decline_count`, `mscdex_last_decline_reason`, `mscdex_last_resolve_kind`, `mscdex_last_header_bytes` 추가
   * `[x]` `ResolveMscdexBuffer`에 resolve kind out-parameter 추가
   * `[x]` DPMI `0300h/1510h` 및 `INT 2Fh AX=1510h` 경로에서 기록 (VEH-safe: Interlocked/단순 대입만, `HandleMscdexRequest` 공통 진입점에서 기록)
   * `[x]` `Win32SharedLiveTelemetry` version 10 필드 4종 + supervisor 스냅샷 출력 확장
   * `[x]` attempt 요약(main.cpp) 출력 라인 추가
3. `[x]` 진단 구동 (임시 로컬 실험 하) → 거절 사유 확정: 프레임 ES를 `0x24`(DS 슬롯)에서 오독하여 ES=0 → 헤더 길이 부족(reason=2)
4. `[x]` Phase B — 수정: `kFrameEsOffset` `0x24`→`0x22`, FLAGS를 16비트 word로 교정 (설계 후보 표 밖의 제4 원인이었음)
5. `[x]` 재구동으로 `mscdex_request_count = 1`, command `03h`, status `0x0100` 확인
6. `[x]` 실험 패치 제거 후 최종 빌드 검증 + 두 백엔드 무실험 회귀 확인(기존 거동과 동일)
7. `[x]` `docs/analysis/pumpit1-mscdex-cd-audio.md`, `docs/analysis/current-execution-frontier.md`, `docs/kb/important-interrupts.md` 갱신 (trap 7.2초 fatal 종료 관찰 포함)
8. `[x]` 작업 로그 작성 (`docs/work-logs/20260716-211-mscdex-real-mode-request-hle-log.md`)

## 검증 기준

* 전체 빌드 통과.
* Phase A 텔레메트리로 거절 사유 1건 이상 확정.
* Phase B 적용 후 동일 조건에서 request가 처리되어 status `0x0100` 기록(가능하면 command `84h` Play 도달).

---

# Work Order: MSCDEX Real-Mode Request Decline Diagnosis and Resolution Fix (Task 211)

Design: `docs/design/20260716-211-mscdex-real-mode-request-hle.md`. Branch: `feature/211-mscdex-real-mode-request-hle`.

Background: the first real PIU MSCDEX `AX=1510h` request (via a DPMI `0300h` frame) was declined at the top of `HandleMscdexRequest` during the 2026-07-16 300-second observation, and current telemetry cannot distinguish the decline reason. Reproduction constraint at task start: on merged main, aot-dynamic is stuck in the `0x030F3438` assertion storm (progress=0) and the default trap backend self-terminates at ~7 s through the `INT 21h AX=4C01h` fatal path — neither reaches MSCDEX. Diagnosis/verification runs therefore use the documented, uncommitted local experiment that disables the physical-register preference in `ReadGuestSegmentSelector`.

Items: (1) design doc [done]; (2) Phase A instrumentation (ThreadContext fields, `ResolveMscdexBuffer` resolve-kind out-parameter, recording on both request paths, telemetry version 10 + supervisor output, attempt summary lines); (3) diagnosis run under the local experiment; (4) Phase B evidence-based fix; (5) re-run confirming `mscdex_request_count ≥ 1` with status `0x0100`; (6) remove the experiment patch and final build; (7) update the MSCDEX and frontier analysis docs (including the trap-backend 7 s fatal-exit observation); (8) work log.
