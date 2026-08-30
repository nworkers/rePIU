# 20260831-548 Linux x64 ucontext adapter 작업 로그

## 한국어

### 결과

Linux x64 `ucontext_t`의 GPR/RIP/RSP/EFLAGS와 packed CS/GS/FS를 현재 32비트
guest context API에 연결했습니다. x64 FXSAVE의 x87 80-bit register bytes와
abridged tag를 기존 FSAVE-style 표현으로 변환하며, host 64비트 instruction을
guest native 경로로 재개하지 않는 경계를 유지합니다. `REG_ESP`/`REG_UESP`를
직접 사용하던 context probe도 x86_64의 `REG_RSP` 경로로 분기했습니다.

### 검증

WSL 재확인 결과 `Ubuntu-24.04`가 WSL2로 실행 중이었습니다. Linux x64 Release
`repiu_exe`와 Release/Debug `repiu_core_probe`의 C++/GAS 빌드·링크가 성공했습니다.
그러나 Debug core probe 실행은 `env_toggle`, `execution_backend`,
`execution_timeout`, `dos_file_handle_cache`를 통과한 뒤 `pit_timer`에서 출력 없이
멈춰 수동 중단했습니다. 따라서 `guest_cpu_context_all`과
`linux_x64_aot_frame_all`의 실행 결과는 아직 미확정입니다.

## English

### Result

Connected Linux x64 `ucontext_t` GPR/RIP/RSP/EFLAGS and packed CS/GS/FS to the existing
32-bit guest-context API. The adapter converts x64 FXSAVE x87 80-bit register bytes and
the abridged tag into the existing FSAVE-style representation while preserving the
boundary that prevents resuming a host 64-bit instruction through the native guest path.
The context probe now uses `REG_RSP` on x86_64 instead of directly referencing the
i386-only `REG_ESP`/`REG_UESP` fields.

### Verification

WSL was rechecked and `Ubuntu-24.04` was running as WSL2. The Linux x64 Release
`repiu_exe` and the Release/Debug `repiu_core_probe` C++/GAS builds and links succeeded.
The Debug core probe passed `env_toggle`, `execution_backend`, `execution_timeout`, and
`dos_file_handle_cache`, then produced no output at `pit_timer` and was manually
interrupted. The execution results for `guest_cpu_context_all` and
`linux_x64_aot_frame_all` therefore remain unresolved.
