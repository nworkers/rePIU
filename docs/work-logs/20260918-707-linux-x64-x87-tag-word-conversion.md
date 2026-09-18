# Task 707 작업 로그 — Linux x64 x87 tag word 변환

설계: [20260918-707](../design/20260918-707-linux-x64-x87-tag-word-conversion.md) ·
작업 지시: [20260918-707](../work-orders/20260918-707-linux-x64-x87-tag-word-conversion.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
배경: [x87 상태와 tag word](../kb/x87-state-and-tag-words.md)

## 수행 결과

Linux x86-64 signal 문맥과 `GuestCpuContext` 사이의 x87 tag word 변환을
고쳤습니다.

`LoadFloatingSave`는 이제 FXSAVE 축약 tag(`_libc_fpstate::ftw`)를 먼저 읽고,
비트가 0인 물리 레지스터를 내용과 무관하게 `empty`로 확장합니다. 사용 중인
레지스터만 `_st[(j - TOP) & 7]`의 내용을 분류하며, `TOP`은 status word 비트
11..13에서 가져옵니다. `StoreFloatingSave`는 tag가 `empty`일 때만 `ftw`
비트를 0으로 둡니다. `ClassifyFloatingTag`는 지수에서 부호 비트를 제거한 뒤
판정하도록 정정했습니다. 두 방향 모두 Linux 커널의 `twd_fxsr_to_i387` 및
`twd_i387_to_fxsr`와 같은 규칙입니다.

수정 전에는 `ClassifyFloatingTag`가 `empty`를 반환하는 경로 자체가 없어
signal 문맥을 지난 x87 상태가 항상 `ftw = 0xFF`가 됐고, 여덟 레지스터가 전부
사용 중으로 복원됐습니다. 게스트의 다음 `FLD`는 x87 stack overflow가 되고 IE가
마스킹돼 있어 예외 없이 QNaN indefinite가 들어갑니다.

`guest_cpu_context` probe는 x86-64에서도 `TagWord`를 비교합니다. 기존에는
x86-64만 register byte만 비교했고, 결함이 그 구멍으로 지나갔습니다.
`TOP = 3`에 물리 레지스터별 tag가 서로 다른 사례를 추가했습니다.

i386 경로, `GuestCpuContext` 구조, 정수 레지스터 write-back 정책, Task 705의
no-op callback 계약은 바꾸지 않았습니다.

## 검증

### 기전 독립 확인

저장소 밖 40줄 C 프로그램으로 기전을 먼저 확인했습니다. signal handler에서
`ftw`만 `0xFF`로 바꾸고 돌아오면 직후 `3.0 * 4.0`이 달라집니다.

```text
mode=0  3*4 = 12.000000 (0x41400000)  sw=0000
mode=1  handler forced ftw=0xFF
        3*4 = -nan (0xFFC00000)       sw=0041   (IE + C1 = stack overflow)
```

### probe

* Linux x64 Debug `repiu`, `repiu_core_probe` 빌드 성공
* Linux x64 core probe **27/27 성공**, 실패 0
* **음성 확인**: 수정 전 `guest_cpu_context.cpp`로 되돌려 다시 빌드하면 같은
  probe가 `guest_cpu_context_round_trip=false`로 실패합니다. 새 사례는 실제
  회귀 감시입니다.
* Win32 x86 Debug 전체 빌드 종료 코드 0, 모든 target 링크
* Win32 x86 core probe 적용 가능한 **25/25 성공**, Linux x64 전용 4개 group
  의도대로 skip

### 실제 `pumpit2a` 실행

30초 예산, 감시견 해제, `build/linux_x64_debug/repiu`, WSLg 표시.

수정 전 (Task 706 상태, 결정적 재현):

```text
[repiu-live] Glide first triangle vertex 0 dwords: 7FC00000 7FC00000 00000000 ...
[repiu-live] Glide first triangle vertex 1 dwords: 7FC00000 7FC00000 00000000 ...
[repiu-live] Glide first triangle vertex 2 dwords: 7FC00000 7FC00000 00000000 ...
```

수정 후:

```text
[repiu-live] Glide first triangle vertex 0 dwords: 42A00000 43BDF800 ...
[repiu-live] Glide first triangle vertex 1 dwords: 43A7F800 43BDF800 ...
[repiu-live] Glide first triangle vertex 2 dwords: 42A00000 42F80000 ...
```

각각 `(80.0, 379.9375)`, `(335.9375, 379.9375)`, `(80.0, 124.0)`으로 Task 254가
확인한 640x480 화면 좌표 범위입니다.

## 남은 것과 정직한 상태

**이 실행은 깨끗하게 끝나지 않습니다.** 정점이 정상화되면서 실행이 처음으로
LFB 경로에 들어갔고, `grLfbLock`이 건넨 `lfbPtr=0xEC02A530`에 게스트가 기록할
때 SIGSEGV로 끝납니다(`exit=139`). `grLfbLock` handler가 host staging surface
포인터를 `std::uint32_t`로 잘라서 건네기 때문입니다. Win32 x86에서는
무손실이지만 x64에서는 아닙니다.

Task 706 상태는 30초 예산을 채우고 종료했으므로 종료 코드만 보면 회귀처럼
보입니다. 회귀가 아니라, 그때는 정점이 NaN이라 LFB 경로에 도달하지 못했던
것입니다. 다음 작업은 LFB staging surface를 게스트 arena 안에 두는 것입니다.

부수적으로 `docs/kb/README.md` 영어 색인에 빠져 있던 두 항목
(`posix-signal-handler-state-sharing`, `x86-32bit-encodings-in-long-mode`)을
같이 채웠습니다.

---

## English

Design: [20260918-707](../design/20260918-707-linux-x64-x87-tag-word-conversion.md) ·
Work order: [20260918-707](../work-orders/20260918-707-linux-x64-x87-tag-word-conversion.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Background: [x87 state and tag words](../kb/x87-state-and-tag-words.md)

### Result

The x87 tag-word conversion between the Linux x86-64 signal context and
`GuestCpuContext` is fixed.

`LoadFloatingSave` now reads the FXSAVE abridged tag (`_libc_fpstate::ftw`)
first and expands a physical register whose bit is clear to `empty` regardless
of its contents, classifying the contents at `_st[(j - TOP) & 7]` only for
registers in use, with `TOP` taken from status-word bits 11..13.
`StoreFloatingSave` clears the `ftw` bit only for an `empty` tag.
`ClassifyFloatingTag` now masks the sign bit out of the exponent before
judging. Both directions follow the same rules as the Linux kernel's
`twd_fxsr_to_i387` and `twd_i387_to_fxsr`.

Before the fix `ClassifyFloatingTag` had no path that returned `empty` at all,
so x87 state passing through a signal context always came back with
`ftw = 0xFF` and all eight registers restored as in use. The guest's next `FLD`
became an x87 stack overflow, and with IE masked the destination silently
received the QNaN indefinite.

The `guest_cpu_context` probe now compares `TagWord` on x86-64 too — it
previously compared register bytes only there, which is the hole the defect
passed through — and gains a case with `TOP = 3` and a different tag per
physical register.

The i386 path, the `GuestCpuContext` structure, integer register write-back
policy, and the Task 705 no-op callback contract are unchanged.

### Verification

**Mechanism, independently.** A 40-line C program outside this repository
confirmed the mechanism first: changing only `ftw` to `0xFF` in a signal
handler turns the `3.0 * 4.0` immediately after the return from
`12.000000 (0x41400000)` into `-nan (0xFFC00000)`, with `fnstsw` reporting
`0x0041` — IE plus C1, an x87 stack overflow.

**Probes.**

* Linux x64 Debug `repiu` and `repiu_core_probe` built successfully.
* Linux x64 core probe: **27 of 27 passed**, zero failures.
* **Negative check**: restoring the pre-fix `guest_cpu_context.cpp` and
  rebuilding makes the same probe fail with
  `guest_cpu_context_round_trip=false`. The new case is a real regression
  guard, not a check that only passes.
* Win32 x86 Debug full build exited 0 with every target linked.
* Win32 x86 core probe: **25 of 25 applicable groups passed**; the four Linux
  x64-only groups skipped as intended.

**Real `pumpit2a` run** — 30-second budget, watchdog off,
`build/linux_x64_debug/repiu`, displayed through WSLg. Before the fix all three
first-triangle vertices carried `7FC00000` for both x and y. After it they
carry `(80.0, 379.9375)`, `(335.9375, 379.9375)` and `(80.0, 124.0)` — the
640x480 screen coordinates Task 254 confirmed as correct.

### What is left, stated plainly

**The run does not end cleanly.** With the vertices correct, execution entered
the LFB path for the first time and ends in a SIGSEGV (`exit=139`) when the
guest writes through the `lfbPtr=0xEC02A530` that `grLfbLock` handed it. The
handler truncates the host staging-surface pointer to `std::uint32_t`, which is
lossless on Win32 x86 and is not on x64.

The Task 706 state used its full 30-second budget, so by exit code alone this
looks like a regression. It is not: that run never reached the LFB path because
its vertices were NaNs. Placing the LFB staging surface inside the guest arena
is the next task.

Incidentally, two entries missing from the English index in
`docs/kb/README.md` (`posix-signal-handler-state-sharing` and
`x86-32bit-encodings-in-long-mode`) were filled in at the same time.
