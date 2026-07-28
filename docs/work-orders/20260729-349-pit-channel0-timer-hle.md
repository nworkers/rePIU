# 20260729-349 PIT 채널 0 타이머 HLE 작업 지시 / Work order

## 한국어

### 목표

고정 `55ms` INT 8 게시를 제거하고, `PIU.EXE`가 PIT 포트에 기록한 채널 0
분주값으로 실제 IRQ0 주기를 결정합니다.

### 작업

1. 플랫폼 공용 `PitChannel0` 설정 상태와 `PitIrqSchedule`을 추가합니다.
2. Win32 thread context에 공용 PIT 상태를 연결합니다.
3. 포트 `0x43`/`0x40`의 8비트 출력을 처리하고 공유 `OUT` 명령은 NOP으로
   변경하지 않습니다.
4. 호스트 폴러에서 BDA 기본 tick과 프로그래밍된 IRQ0 주기를 분리합니다.
5. 분주값, generation, 정확한 240Hz 계산과 scheduler 동작 probe를 추가합니다.
6. 실제 설정을 확인할 수 있는 저빈도 PIT 로그를 추가합니다.
7. Win32 x86 Debug 빌드, AOT probe, 실제 `pumpit1` 실행을 검증합니다.
8. 관련 architecture/analysis 문서와 작업 로그를 갱신합니다.

### 완료 조건

- `0x36`, `0x6C`, `0x13`이 divisor `4,972`, IRQ0 `240Hz`로 반영됩니다.
- 두 tick 대기의 명목 주기가 약 `8.33ms`가 됩니다.
- PIT 설정 전 기본 IRQ0는 divisor `65536`에서 동작합니다.
- BDA `0x46C`는 별도의 기본 BIOS tick으로 유지됩니다.
- 기존 IF gate, pending 병합, AOT safe point, 원본 ISR/IRETD가 유지됩니다.
- 빌드와 probe가 통과하고 실제 실행에서 PIT 설정과 지속 진행이 확인됩니다.
- 코드·문서 변경이 하나의 작업 커밋으로 남습니다.

---

## English

### Goal

Replace the fixed `55ms` INT 8 publisher with an IRQ0 cadence derived from the
PIT channel-0 divisor written by `PIU.EXE`.

### Work

Add shared PIT configuration and scheduling state, connect it to the Win32
thread context and port trap, handle byte writes to `0x43`/`0x40` without
NOP-patching the shared `OUT` helper, separate the BIOS BDA tick from
programmed IRQ0, add probes and concise configuration logging, then verify the
Win32 x86 Debug build, full AOT probe, and a real `pumpit1` run. Update the
relevant architecture, analysis, and work-log documents.

### Completion

The observed bytes produce divisor `4,972` and `240Hz`; the nominal two-tick
wait becomes about `8.33ms`; the default divisor is `65536`; BDA `0x46C`
remains a separate BIOS-rate counter; existing interrupt safety and original
ISR execution remain intact; build, probes, and live execution pass; and the
task is committed as one unit.
