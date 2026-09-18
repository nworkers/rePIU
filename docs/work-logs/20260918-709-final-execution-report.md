# Task 709 작업 로그 — 회수 거절 경로의 최종 실행 요약

설계: [20260918-709](../design/20260918-709-final-execution-report.md) ·
작업 지시: [20260918-709](../work-orders/20260918-709-final-execution-report.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-708](20260918-708-glide-lfb-guest-addressable-storage.md) ·
근거: [20260827-507](20260827-507-linux-shutdown-recovery.md) ·
[20260828-508](20260828-508-refused-recovery-teardown.md)

## 왜 이 작업이 필요했는가

Task 708 뒤 Linux x64는 30초를 완주하지만 화면은 검습니다.
`REPIU_GLIDE_DRAW_DIAG=1`로 Win32 x86과 같은 장면을 비교하니 Glide gate 열이
**#1부터 #50까지 ordinal과 반환 주소가 완전히 일치**하다가 #51에서 갈라집니다.
Win32는 `_GRTEXTEXTUREMEMREQUIRED@8` → `_GRTEXDOWNLOADMIPMAPLEVEL@32` →
clamp/filter/mipmap/source/combine/hint로 텍스처를 올리고, Linux는 그 블록 전체를
건너뜁니다. 텍스처가 없으니 정점 s/t가 0이고 combine이 다르며, 사각형은 검은색으로
검은 배경에 그려집니다.

다음 질문은 "왜 건너뛰는가"이고 그것을 보려면 게스트의 DOS 파일 접근을 봐야
하는데, **Linux 로그에는 그 증거가 한 줄도 없었습니다.** 같은 실행에서 Win32가
945줄, Linux가 133줄이었습니다.

## 확인된 원인

그 줄들은 loader의 최종 요약(`PrintExecutionAttempt`)이 출력하는데, Linux 실행이
그 지점에 도달하지 못합니다. 종료 블록은 게스트 스레드 회수 여부로 갈리고, 거절된
갈래는 `_Exit`로 끝나 `attempt`를 호출자에게 돌려주지 않습니다. Linux x64는
**모든 실행에서** 이 갈래로 내려갑니다(`recovered=0`).

Task 507/508이 이 갈래를 `_Exit`로 만든 이유는 옳습니다. 아직 실행 중인 게스트
스레드 아래에서 fault handler를 떼면 다음 INT3나 trap flag를 받을 것이 없어 커널
기본 처분이 코어를 덤프합니다. **바꿔야 하는 것은 정리 작업이 아니라 보고였습니다.**

## 수행 결과

새 단위 `repiu::engine::FinalExecutionReport`가 "어떻게 보고할지"를 등록받고,
종료 블록이 어느 갈래로 내려가든 떠나기 전에 한 번 부릅니다. host는
`PrintExecutionAttempt`를 부르는 콜백을 등록하고, 정상 경로의 기존 출력도 같은
emit을 지나가게 해 중복을 막았습니다. 등록이 없으면 무동작입니다.

거절 갈래에서는 **게스트 스레드가 살아 있어도 안전한 것만** 채웁니다.
`CopyThreadObservationToAttempt`(`live_telemetry_snapshot.cpp` 927–2351행)는
스칼라와 POD 배열만 복사하고 `std::string`은 건드리지 않음을 확인했습니다.
`attempt`의 문자열 필드는 게스트 스레드가 HLE 처리 중에 쓰므로 일부러 손대지
않았고, `message`에는 이 갈래에서 왔음을 말하는 고정 문자열을 넣었습니다.

Task 507/508의 정리 정책과 종료 코드는 바꾸지 않았습니다. fault handler 제거,
Glide close, translation worker join, page protection 복원은 여전히 이 갈래에서
하지 않습니다.

## 검증

* Linux x64 Debug 빌드, core probe **29/29 성공**(신규 group 포함)
* Win32 x86 Debug 전체 빌드, core probe **27/27 성공**
* 새 `final_execution_report` group: 등록 없이 emit해도 안전하고 보고를
  소비하지 않는지, 등록 뒤 정확히 한 번만 불리는지, 등록 해제가 보고를 막는지,
  reset이 등록과 표식을 함께 지우는지

### Linux x64 실행 (30초, `pumpit2a`)

```text
전: err_lines=133,  DOS file I/O 0건,  DOS path trace 0건
후: err_lines=968,  DOS file I/O 64건, DOS path trace 16건
[repiu-shutdown] step=probe-dump
[repiu-shutdown] step=final-report      ← 신규
... 요약 686줄 ...
[repiu-shutdown] step=immediate-exit
[loader] Win32 minimal execution message: guest thread was not recovered;
         summary reported from the immediate-exit path
```

### Win32 x86 실행 (20초, `pumpit2a`)

`Win32 minimal execution attempt:` 줄이 **정확히 1회**. 종료는 여전히
`glide-close → fault-handler → translation-worker → write-watches →
probe-dump → thread-release → done`으로 정상 갈래를 탑니다(`recovered=1`).

### 음성 확인에 대해

Task 708과 같은 이유로 "수정 전 구현에서 새 probe가 실패한다"는 보이지
못했습니다. 이 seam은 이번에 처음 생긴 것이라 이전 코드에는 호출할 대상이
없습니다. before/after 증거는 위 Linux 실행의 133줄 대 968줄입니다.

## 이 작업이 바로 드러낸 것

instrument가 켜지자마자 다음 작업의 대상이 나왔습니다. 두 host의 DOS 경로 해석은
**완전히 동일**하고(`chdir` 성공 9건 / 실패 7건, 같은 경로), 파일 내용도
동일합니다 — 마지막 64개 read의 offset과 prefix 바이트가 바이트 단위로 같습니다.
다른 것은 **횟수**입니다.

| | Win32 x86 | Linux x64 |
|---|---:|---:|
| `DOS file I/O trace observed` | 147 | **1,110,383** |

같은 30초 동안 Win32는 파일 연산 147회로 텍스처를 올리고 렌더하는데, Linux는
110만 회를 수행하고도 텍스처 블록에 닿지 못합니다. 마지막으로 읽던 파일은 두
host 모두 `PIU\DATAS\MODEL\NONSTOP.CAM`입니다. 게스트가 파일 읽기 루프에서
빠져나오지 못하는 것으로 보이며, 이것이 Task 710의 대상입니다.

이 작업은 그 원인을 **답하지 않습니다.** 볼 수 있게 만든 것이 전부입니다.

---

## English

Design: [20260918-709](../design/20260918-709-final-execution-report.md) ·
Work order: [20260918-709](../work-orders/20260918-709-final-execution-report.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-708](20260918-708-glide-lfb-guest-addressable-storage.md) ·
Basis: [20260827-507](20260827-507-linux-shutdown-recovery.md) ·
[20260828-508](20260828-508-refused-recovery-teardown.md)

### Why this was needed

After Task 708 the Linux x64 run completes its 30 seconds with a black screen.
Comparing the same scene against Win32 x86 under `REPIU_GLIDE_DRAW_DIAG=1`, the
Glide gate sequence matches **exactly — ordinal and return address — for the
first 50 gates** and then diverges at #51: Windows uploads a texture through
`_GRTEXTEXTUREMEMREQUIRED@8`, `_GRTEXDOWNLOADMIPMAPLEVEL@32` and the
clamp/filter/mipmap/source/combine/hint block, and Linux skips that block
entirely. With no texture the vertex s/t are zero and the combine differs, so
the quads are drawn black on black.

The next question is why, and answering it means looking at the guest's DOS
file access — of which **the Linux log contained not one line**. The same run
wrote 945 lines on Win32 and 133 on Linux.

### Confirmed cause

Those lines come from the loader's final summary, `PrintExecutionAttempt`, and
the Linux run never reaches it. The shutdown block branches on whether the
guest thread was recovered, and the refused arm ends in `_Exit` without
returning `attempt` to its caller. Linux x64 takes that arm on **every** run
(`recovered=0`).

The reason Tasks 507 and 508 made that arm exit is correct: removing the fault
handler under a still-running guest thread leaves its next INT3 or trap flag
with nothing to receive it, and the kernel's default disposition dumps core.
**What had to change was the reporting, not the cleanup.**

### Result

A new `repiu::engine::FinalExecutionReport` unit takes a registration of how to
report, and the shutdown block calls it once before leaving on whichever arm it
takes. The host registers a callback that calls `PrintExecutionAttempt`, and
the normal path's existing print goes through the same emit so nothing prints
twice. With nothing registered, emitting does nothing.

The refused arm fills in **only what is safe to read beside a live guest
thread**. `CopyThreadObservationToAttempt` (`live_telemetry_snapshot.cpp`,
lines 927–2351) was checked and copies only scalars and POD arrays, touching no
`std::string`. The string members of `attempt` are deliberately left alone
because the guest thread writes them while servicing HLE, and `message` gets a
fixed string naming this arm.

The Task 507/508 cleanup policy and the exit code are unchanged: the fault
handler, the Glide close, the worker join and the page protections are still
not touched on this arm.

### Verification

* Linux x64 Debug build, core probe **29 of 29**, including the new group.
* Win32 x86 Debug full build, core probe **27 of 27**.
* The new `final_execution_report` group covers emitting with nothing
  registered (safe, and does not consume the one report), a registered callback
  invoked exactly once, a cleared registration stopping the report, and reset
  clearing both the registration and the flag.

A 30-second Linux x64 `pumpit2a` run went from 133 stderr lines with no DOS
traces to 968 lines with 64 file-I/O and 16 path traces, printing
`step=final-report` between `probe-dump` and `immediate-exit` and closing with
`guest thread was not recovered; summary reported from the immediate-exit
path`. A 20-second Win32 x86 run printed the summary exactly once and still
took the normal arm end to end.

**About the negative check.** As in Task 708, the new probe cannot be shown
failing against the pre-fix implementation, because this seam is new and the
earlier code has nothing to call. The before/after evidence is the 133-line
against 968-line Linux run.

### What it immediately exposed

The instrument produced the next task's target as soon as it was switched on.
DOS path resolution is **identical** on the two hosts — nine `chdir` successes
and seven failures, the same paths — and so is the file content: the last 64
reads agree byte for byte in offset and prefix. What differs is the **count**:
`DOS file I/O trace observed` is 147 on Win32 and **1,110,383** on Linux x64. In
the same thirty seconds Win32 uploads its texture and renders after 147 file
operations, while Linux performs over a million and never reaches the texture
block. The file both hosts were last reading is
`PIU\DATAS\MODEL\NONSTOP.CAM`. The guest appears not to leave a file-reading
loop, and that is Task 710.

This task does not answer why. Making it visible was the whole of it.
