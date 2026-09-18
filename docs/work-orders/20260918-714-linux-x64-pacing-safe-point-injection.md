# Task 714 작업 지시 — 화면 비교와 x64 safe point 틱 주입

설계: [20260918-714](../design/20260918-714-linux-x64-pacing-safe-point-injection.md)

## 범위

두 host의 화면 진행을 비교하고, Linux x64에서 safe point 틱이 주입되지 않는 원인을
고친다. 고친 주입이 실행을 불안정하게 만들면 opt-in으로 두고 기본 동작은 유지한다.

## 단계

1. `REPIU_GLIDE_PIXEL_DIAG`로 두 host 30초 비교, 틱 통계 비교
2. x64 `InjectPendingInterrupts`: cache `eip`를 게스트 주소로 바꿔 선택자 조회와
   프레임에 사용(`REPIU_LINUX_X64_SAFE_POINT_INJECTION` opt-in)
3. Linux 처리되지 않은 폴트 줄에 폴트 종류와 `si_code`
4. 작업 로그, frontier

## 검증

* 두 host core probe, Win32 전체 빌드
* Linux off/on 실행 비교

## 완료 조건

* 기본 동작 회귀 없음
* 관측·원인·남은 크래시가 기록됨

---

## English

Design: [20260918-714](../design/20260918-714-linux-x64-pacing-safe-point-injection.md)

### Scope

Compare screen progression across the hosts and fix why Linux x64 injects no ticks
at safe points. If the fixed injection destabilizes execution, keep it opt-in and
leave the default unchanged.

### Steps

1. Compare 30-second runs on both hosts under `REPIU_GLIDE_PIXEL_DIAG`, and their
   tick statistics.
2. In x64 `InjectPendingInterrupts`, replace a cache `eip` with its guest address for
   the selector lookup and the frame (opt-in
   `REPIU_LINUX_X64_SAFE_POINT_INJECTION`).
3. Add the fault kind and `si_code` to the unhandled Linux fault line.
4. Work log and frontier.

### Verification

* Core probes on both hosts; full Win32 build.
* Linux off/on runs compared.

### Done when

* The default behavior has not regressed.
* The observation, causes and remaining crash are recorded.
