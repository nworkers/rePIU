# 20260831-549 Linux x64 core probe 정지 지점 분리 작업 지시서

## 한국어

### 목적

Task 547과 Task 548이 두 번 연속으로 미확정으로 남긴 Linux x64 core probe 실행을
완료합니다. 두 작업 로그는 모두 실행이 `pit_timer`에서 멈췄다고 기록했고, 그 결과
`guest_cpu_context_all`과 `linux_x64_aot_frame_all`의 실행 결과가 없습니다.

### 먼저 확인할 것

`pit_timer`가 실제 정지 지점인지 확인합니다. `repiu_core_probe`는 `std::cout`에
개행만 쓰고 flush하지 않으므로, Windows 세션에서 `wsl.exe`를 통해 실행하면 stdout이
pipe가 되어 block buffering 됩니다. 이 경우 마지막으로 보인 출력은 멈춘 지점이
아니라 마지막으로 flush된 경계입니다. `pit_timer` 자체에는 loop도, 대기도, syscall도
없으므로 이 귀속은 어느 쪽이든 성립하지 않습니다.

### 작업

- core probe 출력을 unbuffered로 만들어 "출력이 멈춘 곳 = 실행이 멈춘 곳"을 성립시킨다.
- Linux x64 Debug `repiu_core_probe`를 빌드하고 실제 정지 지점을 특정한다.
- 정지 원인을 분리하여 수정하거나, 수정이 다음 단위에 속하면 근거와 함께 기록한다.
- Linux x64에서 반복 가능한 빌드 절차를 스크립트로 남긴다.

### 검증

Linux x64 `repiu_core_probe`를 실행하여 `guest_cpu_context_all`,
`linux_x64_aot_frame_all`, `core_probe_all`의 실제 값을 기록합니다. Linux i386
`repiu_core_probe`와 Win32 `repiu_core_probe`도 회귀로 빌드·실행합니다.

## English

### Objective

Complete the Linux x64 core-probe run that Task 547 and Task 548 both left unresolved.
Both work logs recorded that the run stopped at `pit_timer`, which is why
`guest_cpu_context_all` and `linux_x64_aot_frame_all` have no measured value.

### What to check first

Establish whether `pit_timer` is really where the process stopped. `repiu_core_probe`
writes newlines to `std::cout` and never flushes, so running it through `wsl.exe` from a
Windows session makes stdout a pipe and buffers it in whole blocks. The last visible line
is then not the stopping point but the last flushed boundary. `pit_timer` itself holds no
loop, no wait, and no syscall, so that attribution could not have been right either way.

### Work items

- Make the core probe's output unbuffered, so that where the output stops is where the
  run stopped.
- Build the Linux x64 Debug `repiu_core_probe` and identify the real stopping point.
- Isolate and fix the cause, or record it with evidence if the fix belongs to a later
  unit.
- Leave a repeatable Linux x64 build procedure as a script.

### Verification

Run the Linux x64 `repiu_core_probe` and record the measured values of
`guest_cpu_context_all`, `linux_x64_aot_frame_all`, and `core_probe_all`. Build and run
the Linux i386 and Win32 `repiu_core_probe` as regressions.
