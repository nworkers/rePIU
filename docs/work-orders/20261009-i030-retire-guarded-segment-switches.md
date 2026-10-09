# 작업 지시: guarded segment load·pop·read 끄기 스위치 삭제 (issue #30)

설계: `docs/design/20261009-i030-retire-guarded-segment-switches.md`

1. `src/host/loader/main.cpp`의 세 스위치 읽기를 `use_dynamic_backend`로 바꾼다.
2. 문서: ARCHITECTURE pop·load 절, frontier Task 384 기록, 환경 변수 목록, TODO.
3. 검증: Win32·Linux x64 Release 빌드, aot_probe 전체 체인, core probe, 변수 읽기 `grep`.

완료 기준: 코드가 `REPIU_AOT_GUARDED_SEGMENT_LOAD/POP/READ`를 읽지 않고, 변수 없이 실행했을 때의
빌드 옵션 값이 변경 전과 같다.

---

# Work order: retire the guarded segment load, pop and read kill switches (issue #30)

(1) Replace the three switch reads in the loader with `use_dynamic_backend`. (2) Update the
ARCHITECTURE pop and load sections, the frontier's Task 384 record, the inventory and the TODO.
(3) Verify: Win32 and Linux x64 Release builds, the aot_probe chain, the core probe, `grep`. Done
when the code no longer reads the three variables and the build options with nothing set match
the previous build.
