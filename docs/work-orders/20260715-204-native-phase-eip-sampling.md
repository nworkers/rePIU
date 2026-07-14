# 네이티브 구간 EIP 샘플링 텔레메트리 작업 지시서
# Native-Phase EIP Sampling Telemetry Work Order

## 1. 작업 개요 (Task Overview)
* **목적:** 디스패치가 0인 네이티브 실행 구간에서 게스트 스레드 EIP를 주기 샘플링하여, 600초 관측에서 미확정으로 남은 150초 이후 상태(폴링 대기 / 번역 결함 무한 루프 / 장시간 연산)를 판정할 수 있는 텔레메트리를 추가합니다.
* **관련 문서:** `docs/design/20260715-native-phase-eip-sampling.md`, `docs/analysis/current-execution-frontier.md` (2026-07-14 600초 관측 항목)

* **Goal:** Add periodic guest-thread EIP sampling during dispatch-silent native phases so the unresolved post-150 s state from the 600-second observation (polling wait / translated-code infinite loop / long compute) becomes decidable.
* **References:** `docs/design/20260715-native-phase-eip-sampling.md`, `docs/analysis/current-execution-frontier.md` (2026-07-14 entry)

---

## 2. 세부 구현 대상 (Detailed Tasks)

### 1) live_telemetry.h — 공유 텔레메트리 확장
* version 8 → 9.
* 최신 샘플 필드(`native_sample_count`, `native_sample_unmapped_count`, `native_sample_eip`, `native_sample_guest_eip`, 정수 레지스터 8종, `native_sample_eflags`)와 최근 8개 게스트 주소 링(`native_sample_ring[8]`, `native_sample_ring_mapped_bits`, `native_sample_ring_cursor`)을 추가합니다.

### 2) native_phase_sampler.{h,cpp} — 샘플러 신설 (src/platform/win32/)
* suspend → `GetThreadContext(CONTEXT_CONTROL|CONTEXT_INTEGER)` → (EIP가 cache 범위 안일 때만 `FindAotGuestAddress` 역매핑) → resume 순서의 캡처 함수.
* suspend~resume 사이 heap 할당·lock·I/O 금지. resume 이후 공유 텔레메트리 게시와 `[repiu-sample]` stderr 한 줄 출력.
* 샘플러 상태(마지막 샘플 tick, 링)는 호출자 소유 구조체로 관리합니다.

### 3) execution_trampoline.cpp — PollThreadUntilExit orchestration
* 기존 progress 추적이 1,000ms 이상 무진행일 때만 500ms 주기로 샘플러를 호출합니다.
* 진행 재개 시 quiet 리셋에 의해 샘플링이 자동 중단되는 것을 확인합니다.

### 4) supervisor_main.cpp — 스냅샷 출력
* `PrintSnapshot`에 신규 필드(누적/미매핑 샘플 수, 최신 EIP/게스트 EIP, 링과 매핑 비트)를 출력합니다.

* Extend `Win32SharedLiveTelemetry` to version 9 with latest-sample registers and an 8-entry guest-EIP ring; add a dedicated sampler in `src/platform/win32/native_phase_sampler.{h,cpp}` (suspend → capture → conditional reverse map → resume, no alloc/lock/IO while suspended); orchestrate from `PollThreadUntilExit` (sample every 500 ms only after ≥1,000 ms of progress silence); print the new fields from the supervisor snapshot.

---

## 3. 검증 방법 (Verification Procedure)
* `scripts/build_win32_x86.ps1`로 win32_x86_debug 빌드가 오류 없이 통과함을 확인합니다.
* `REPIU_EXECUTION_TIMEOUT_MS=0`, `REPIU_EXECUTION_BACKEND=aot-dynamic` 환경에서 `repiu_supervisor_win32.exe pumpit1 240000`을 구동하여 다음을 확인합니다.
  1. 자산 사이클 네이티브 구간과 150초 이후 무디스패치 구간에서 `[repiu-sample]` 라인과 supervisor 스냅샷 링이 채워진다.
  2. 디스패치 활발 구간에서는 샘플 수가 증가하지 않는다.
  3. 디스패치 entry/exit 균형, 게스트 fatal 부재 등 기존 실행 결과에 회귀가 없다.
* 수집된 링의 게스트 주소 분포로 폴링 대기 / 무한 루프 / 장시간 연산 여부를 판정하고 `docs/analysis/current-execution-frontier.md`에 기록합니다.
