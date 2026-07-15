# 작업 로그: MSCDEX real-mode 요청 거절 진단 및 해석 수정 (Task 211)

* 작업 지시: `docs/work-orders/20260716-211-mscdex-real-mode-request-hle-order.md`
* 설계: `docs/design/20260716-211-mscdex-real-mode-request-hle.md`
* 브랜치: `feature/211-mscdex-real-mode-request-hle`

## 1. 작업 개요

2026-07-16 300초 관측(99f60de 기준선)에서 PIU의 첫 MSCDEX `AX=1510h` 요청(DPMI `0300h` 프레임 경유)이 `HandleMscdexRequest` 초입에서 거절되는 것이 발견되었다. 본 작업은 거절 사유 진단 계측(Phase A)을 추가하고, 확정된 원인을 수정(Phase B)했다.

## 2. 구현 내용

### Phase A — 진단 계측

* `ThreadContext`: `mscdex_frame_es`, `mscdex_decline_count`, `mscdex_last_decline_reason`(1=버퍼 해석 실패, 2=헤더 길이 부족), `mscdex_last_resolve_kind`(1=selector, 2=real-mode), `mscdex_last_header_bytes` 추가.
* `ResolveMscdexBuffer`: 해석 경로를 보고하는 `resolve_kind` out-parameter 추가.
* `HandleMscdexRequest`: 진입 시 위 필드와 공유 텔레메트리를 기록 (VEH-safe: Interlocked/단순 대입만 사용).
* `Win32SharedLiveTelemetry` version 9→10: `mscdex_frame_es`/`mscdex_decline_reason`/`mscdex_resolve_kind`/`mscdex_header` 추가, supervisor 스냅샷에 `mscdex_es/kind/reason/header=` 출력 확장.
* attempt 요약(main.cpp): `Win32 MSCDEX request ES/resolve kind/declines/reason/header` 라인 추가.

### Phase B — 원인 수정

* **원인 (확정):** DPMI `AX=0300h` real-mode register 구조에서 ES를 스펙 오프셋 `0x22`가 아닌 `0x24`(DS 슬롯)에서 읽어 항상 0을 얻었고, segment 0이 zero-init 저메모리 backing으로 해석되어 `request[0] < 13` 검사에서 거절되었다.
* `kFrameEsOffset`을 `0x24` → `0x22`로 교정하고, FLAGS 읽기/쓰기를 dword에서 스펙대로 16비트 word(`0x20`)로 교정했다.
* 관련 지식은 `docs/kb/important-interrupts.md`의 INT 31h `AX=0300h` 구조 표(출처 링크 포함)로 정리했다.

## 3. 검증

### 재현 제약과 진단 실험

머지된 main에서는 두 백엔드 모두 MSCDEX에 도달하지 못함을 먼저 확인했다.

* aot-dynamic 90초: `0x030F3438` assertion 폭풍, `progress=0`, 약 137k dispatch/s.
* trap(기본) 600초 의도 구동: **약 7.2초에 게스트가 `INT 21h AX=4C01h`(DOS/4G fatal 종료)로 자체 종료** (`exception=0x80000004`, `EAX=0x4C01`, 직전 바이트 `B4 4C CD 21`).

따라서 설계에 명시한 대로 **진단용 임시 로컬 실험**(`ReadGuestSegmentSelector`의 물리 레지스터 우선 반환을 작업 사본에서만 무효화, 커밋하지 않음)을 적용해 99f60de 수준의 도달성을 복원한 상태에서 진단·검증 구동을 수행했다.

### 결과

| 구동 | 조건 | 결과 |
| --- | --- | --- |
| 진단 1 (90s, aot-dynamic, 실험) | Phase A만 | `mscdex_es/kind/reason/header = 0x0/2/2/0x0` — ES=0 판독, real-mode 해석, 헤더 길이 부족 거절 확정 |
| 진단 2 (60s, aot-dynamic, 실험) | Phase A+B | `mscdex_es/kind/reason/header = 0x100/2/0/0x3001a`, `request/cmd/status = 1/3/0x100` — **요청 처리 성공** (command `03h` IOCTL INPUT, status done) |
| 회귀 (30s, aot-dynamic, 실험 없음) | 최종 빌드 | 기존과 동일한 `0x030F3438` 정지 (`progress=0`) — 신규 회귀 없음 |
| 회귀 (20s, trap, 실험 없음) | 최종 빌드 | 기존과 동일한 7.2초 `AX=4C01h` 종료, dispatch 781,653으로 결정적 동일 — 신규 회귀 없음 |

빌드는 `build\win32_x86_debug` 전체 구성이 에러 없이 통과했다(사전 존재하는 spdlog 코드페이지 경고 C4819만 발생).

### 게스트가 전달한 실제 값 (수정 후)

* 프레임 ES `0x0100` = DPMI `AX=0100h`로 할당된 real-mode 블록(bump base `0x1000`). 게스트의 26바이트 요청 패킷이 backing에 정상 기록되어 있었다(`0x1A 0x00 0x03 …`).
* 즉 DPMI `0100h` 할당·selector 기반 패킷 쓰기·real-mode 해석 경로는 모두 정상이었고, 프레임 ES 오독이 유일한 결함이었다.

## 4. 남은 항목 / 후속 제안

1. CD-DA `84h`(play) 도달 확인은 게임 실행이 곡 재생 단계까지 진행해야 가능하다. main의 `0x030F3438` 정지(Task 210)와 trap 백엔드 7.2초 fatal 종료가 선행 blocker다.
2. trap 백엔드의 7.2초 `4C01h` 종료는 Task 209의 "trap 30초 정상 진행" 기록과 상충한다 — Task 210 검증 시 판정 조건/빌드 상태 차이를 재확인해야 한다 (frontier 문서에 미확정으로 기록).
3. 문서 갱신: `docs/analysis/current-execution-frontier.md`(Task 211 항목), `docs/analysis/pumpit1-mscdex-cd-audio.md`(첫 실제 요청과 수정), `docs/kb/important-interrupts.md`(INT 31h `0300h` 구조).

---

# Work Log: MSCDEX Real-Mode Request Decline Diagnosis and Resolution Fix (Task 211)

**Summary.** The first real PIU MSCDEX `AX=1510h` request (via a DPMI `0300h` frame), discovered in the 2026-07-16 300-second baseline, was being declined at the top of `HandleMscdexRequest`. Phase A added VEH-safe decline diagnostics (`ThreadContext` fields, a resolve-kind out-parameter on `ResolveMscdexBuffer`, shared telemetry version 10 with supervisor output, and attempt-summary lines). Phase B fixed the confirmed root cause: the DPMI real-mode register structure's ES field was read at offset `0x24` (the DS slot) instead of the spec's `0x22`, always yielding 0, which resolved to zero-initialized low memory and failed the header-length check; FLAGS was also corrected from a dword read/write to the spec's 16-bit word at `0x20`.

**Verification.** On merged main neither backend reaches MSCDEX (aot-dynamic: `0x030F3438` assertion storm at progress=0; default trap: deterministic guest self-termination at ~7.2 s via `INT 21h AX=4C01h`, 781,653 dispatches), so diagnosis/verification ran under the documented uncommitted local experiment disabling the physical-register preference in `ReadGuestSegmentSelector`. Results: diagnosis run pinned `ES=0 / kind=2 / reason=2`; after the fix the same conditions yield `ES=0x100 / reason=0 / header=0x0003001A` and a handled request (`request/cmd/status = 1/3/0x100`, command `03h` IOCTL INPUT). No-experiment regression runs reproduce the pre-existing behavior on both backends exactly. The full debug build passes with only the pre-existing spdlog C4819 warning.

**Remaining.** CD-DA play (`84h`) confirmation requires execution to reach actual song playback (blocked by Task 210 and the trap-backend early fatal exit); the trap backend's 7.2 s `4C01h` termination contradicts Task 209's "normal 30 s progression" reading and needs re-checking during Task 210.
