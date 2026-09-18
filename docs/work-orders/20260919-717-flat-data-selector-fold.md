# Task 717 작업 지시 — loader 데이터 선택자도 flat으로 fold

설계: [20260919-717](../design/20260919-717-flat-data-selector-fold.md)

## 범위

주입 on Linux x64의 27–28초 SIGSEGV(힙이 `0xF186F186`로 덮임)의 주체를 찾고 고친다.

## 단계

1. 진단: `REPIU_LIVE_GUEST_SCAN`, `[repiu-fault-stack]`, `REPIU_LINUX_X64_DATA_WATCH`
2. fill 범위와 쓰는 명령 확인, 두 host에서 같은 지점 레지스터 비교
3. `ApplyFlatSegmentFolds`와 `flat_data_selector`
4. probe
5. 두 host 빌드·core probe, Linux on/off 실행, Linux 90초, Win32 30초
6. 작업 로그, frontier

## 검증

* Linux x64 core probe 30/30, Win32 core probe 28/28
* Linux on: 폴트 0, off: 이전과 같음
* Win32: 이전 실행과 같은 수치

---

## English

Design: [20260919-717](../design/20260919-717-flat-data-selector-fold.md)

### Scope

Find and fix what overwrites the heap with `0xF186F186` and kills Linux x64 with
injection on at 27–28 seconds.

### Steps

1. Diagnostics: `REPIU_LIVE_GUEST_SCAN`, `[repiu-fault-stack]`,
   `REPIU_LINUX_X64_DATA_WATCH`.
2. Establish the fill's extent and its writing instruction; compare registers at
   the same point on both hosts.
3. `ApplyFlatSegmentFolds` and `flat_data_selector`.
4. Probe.
5. Builds and core probes on both hosts, Linux runs on and off, a 90-second Linux
   run, a 30-second Win32 run.
6. Work log and frontier.

### Verification

* Linux x64 core probe 30 of 30, Win32 28 of 28.
* Linux on: no faults; off: unchanged.
* Win32: the same figures as the previous run.
