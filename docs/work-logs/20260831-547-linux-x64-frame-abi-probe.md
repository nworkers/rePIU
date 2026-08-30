# 20260831-547 Linux x64 frame/ABI probe 작업 로그

## 한국어

### 결과

Linux x64 AOT/DBT의 첫 이름 있는 `LinuxX64AotDispatchFrame`을 추가했습니다.
guest register와 metadata는 32비트로, context·memory base·continuation은 64비트
host pointer로 고정하고 C++ offset assertion을 추가했습니다.

synthetic SysV AMD64 probe는 resolver 호출 시 16-byte stack alignment, callee-saved
register, XMM state save/restore, frame edit를 검사합니다. 이 probe는 실제 guest나
production thunk를 실행하지 않습니다.

### 검증

`git diff --check`를 통과했고, WSL2 `Ubuntu-24.04`에서 Linux x64 Release와 Debug
`repiu_core_probe`의 C++/GAS 컴파일·링크가 모두 성공했습니다. 다만 Debug probe
실행은 기존 `dos_file_handle_cache`까지 통과한 뒤 `pit_timer` 단계에서 출력 없이
멈춰 수동 중단했습니다. 따라서 synthetic SysV ABI probe의 실행 결과는 아직
미확정이며, 다음 세션에서는 `pit_timer` hang을 분리한 뒤 ABI probe를 실행해야
합니다.

## English

### Result

Added the first named `LinuxX64AotDispatchFrame` for Linux x64 AOT/DBT. Guest registers
and metadata are fixed at 32 bits, while context, memory-base, and continuation fields
are 64-bit host pointers with C++ offset assertions.

The synthetic SysV AMD64 probe checks 16-byte call-site stack alignment, callee-saved
registers, XMM state save/restore, and frame edits. It never executes a guest or a
production thunk.

### Verification

`git diff --check` passed. On WSL2 `Ubuntu-24.04`, the Linux x64 Release and Debug
`repiu_core_probe` C++/GAS compile and link both succeeded. The Debug probe passed the
existing `dos_file_handle_cache` stage, then produced no output at `pit_timer` and was
manually interrupted. The synthetic SysV ABI probe therefore remains unexecuted; the
next session must isolate the `pit_timer` hang before running the ABI probe.
