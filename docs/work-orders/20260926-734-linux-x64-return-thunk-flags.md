# Task 734: Linux x64 return thunk의 guest EFLAGS 복원 작업 지시

설계: [20260926-734](../design/20260926-734-linux-x64-return-thunk-flags.md)

## 한국어

1. 현재 branch를 확인한다(`work/20260919-718-default-safe-point-injection`).
2. 사용자 로그로 곡 선택 정지의 범위를 좁힌다. 게스트 활동이 타이머 tick에 비례하는지,
   CD 데이터와 LBA 계산이 맞는지 확인한다.
3. 정지 구간을 가르는 계측을 준비한다. 기존 `REPIU_GLIDE_FRAME_RATE_LOG`,
   `REPIU_LINUX_X64_NATIVE_SAMPLE_TRACE`, `REPIU_AOT_CACHE_MAP_TRACE`를 묶은
   `scripts/task734_songselect_stall_capture.sh`를 추가한다.
4. WSLg에서 `SDL_VIDEO_DRIVER=x11`과 XTest 합성 키로 곡 선택까지 자동 재현하고, 회수 지점을
   guest 주소와 번역 바이트로 되돌린다.
5. `RepiuLinuxX64ReturnThunk`가 `jmp r10` 직전에 frame의 guest EFLAGS를 복원하도록 고친다.
6. `linux_x64_guest_register` core probe에 `guest_return_preserves_flags`를 추가하고, 수정 전
   thunk에서 실패하고 수정 후 통과하는지 확인한다.
7. 수정 전후 자동 재현과 Linux x64 core probe로 검증한다.
8. 설계, 작업 로그, `docs/analysis/linux-port-frontier.md`, `docs/EXE_DESIGN.*`,
   `ARCHITECTURE.md`, `docs/kb/dynamic-recompilation-and-aot-dispatch.md`를 갱신하고 커밋한다.

## English

1. Confirm the current branch (`work/20260919-718-default-safe-point-injection`).
2. Narrow the song-select freeze from the user's logs: check whether guest activity is proportional
   to timer ticks, and whether the CD data and LBA arithmetic are correct.
3. Prepare instruments that split the frozen stretch: add
   `scripts/task734_songselect_stall_capture.sh`, which bundles the existing
   `REPIU_GLIDE_FRAME_RATE_LOG`, `REPIU_LINUX_X64_NATIVE_SAMPLE_TRACE` and
   `REPIU_AOT_CACHE_MAP_TRACE`.
4. Reproduce song select automatically on WSLg with `SDL_VIDEO_DRIVER=x11` and XTest synthetic keys,
   and map the recovery point back to a guest address and its translated bytes.
5. Make `RepiuLinuxX64ReturnThunk` restore the frame's guest EFLAGS right before `jmp r10`.
6. Add `guest_return_preserves_flags` to the `linux_x64_guest_register` core probe, and confirm it
   fails with the old thunk and passes with the fix.
7. Verify with automated reproductions before and after the fix and with the Linux x64 core probe.
8. Update the design, work log, `docs/analysis/linux-port-frontier.md`, `docs/EXE_DESIGN.*`,
   `ARCHITECTURE.md` and `docs/kb/dynamic-recompilation-and-aot-dispatch.md`, then commit.
