# 작업 지시: 진단 이름 뒤에 숨은 끄기 스위치와 DOS/DPMI 실험 두 개 삭제 (issue #24)

설계: `docs/design/20261009-i024-retire-hidden-toggles.md`

1. Glide draw 진입점 스위치 — `linexe_glide_boundary.cpp`.
2. timer tick backlog — `timer_tick_delivery.h/.cpp`, `execution_trampoline.cpp`,
   `live_telemetry_snapshot.cpp`, `cd_audio_position_census.h/.cpp`, loader 최종 로그,
   `timer_tick_delivery_probe.cpp`.
3. JAMMA 스냅샷 스위치 — `port_io_emulator.cpp`.
4. PharLap 경로 실험 — `dos_int21_services.h/.cpp`, `instruction_emulation.cpp`.
5. 1E7F 블록 — `dpmi_mscdex_services.cpp`.
6. 스크립트: `task366` 삭제, `task414` 정규식 수정.
7. 문서: ARCHITECTURE, CD census 가이드, frontier 토글 표, I/O 포트 명세, README, 환경 변수
   목록, TODO.
8. 검증: Win32 Release·Linux x64 빌드, probe, 변수 읽기 `grep`. 게임 실행은 사용자 확인 뒤
   한 롬셋만.

완료 기준: 변수 없이 실행했을 때의 동작이 변경 전과 같고, 코드가 6개 변수
(`REPIU_GLIDE_DRAW_ENTRY_POINTS`, `REPIU_TIMER_TICK_BACKLOG`, `REPIU_JAMMA_SNAPSHOT`,
`REPIU_DOS4GW_MEMORY_PATH_PROBE`, `REPIU_DPMI_1E7F_PROBE_SUCCESS`, `REPIU_DPMI_1E7F_TRACE`)를
더 이상 읽지 않는다.

---

# Work order: retire the hidden kill switches and two DOS/DPMI experiments (issue #24)

(1) The Glide draw entry point switch. (2) The timer tick backlog switch, its boolean policy,
arguments, always-zero counters and their output (delivery header and source, trampoline, live
snapshot, CD census, loader final log, probe). (3) The JAMMA snapshot switch. (4) The PharLap
memory path experiment in both `AH=30h` handlers. (5) The `AX=1E7Fh` block. (6) Delete
`task366`, fix the regex in `task414`. (7) ARCHITECTURE, CD census guide, frontier toggle table,
I/O port specification, README, inventory, TODO. (8) Verify: Win32 Release and Linux x64 builds,
probes, `grep` for reads; one short romset run only with the user's go-ahead. Done when behavior with nothing set matches the
pre-change build and the code no longer reads the six variables.
