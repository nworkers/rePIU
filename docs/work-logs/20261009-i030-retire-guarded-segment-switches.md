# 작업 로그: guarded segment load·pop·read 끄기 스위치 삭제 (issue #30)

설계: `docs/design/20261009-i030-retire-guarded-segment-switches.md`
작업 지시: `docs/work-orders/20261009-i030-retire-guarded-segment-switches.md`

## 한 일

* loader의 세 줄(`REPIU_AOT_GUARDED_SEGMENT_POP/LOAD/READ`를 `ResolvePromotedToggle`로 읽던
  것)을 `use_dynamic_backend` 대입으로 바꾸고 승격 경위를 주석 한 묶음으로 합쳤다. 빌드 옵션·
  placement 필드와 loader의 `enabled/sites` 로그 줄은 그대로다.
* 문서: ARCHITECTURE의 pop·load 절의 스위치 문장을 삭제 사실로 바꾸고, frontier의 Task 384
  승격 기록에 한 줄을 덧붙였다. 환경 변수 목록의 "보류" 항목을 삭제로 고치고, TODO의 보류
  항목을 남은 일(판정 미정 실험 2개, 판정하지 않은 진단, 최근 승격 스위치 11개)로 바꿨다.

## 검증

* **빌드**: Win32 Release·Linux x64 Release 모두 오류 0.
* **aot_probe 전체 체인**(`MASTER/PIU_1ST/PIU/PIU.EXE`): 종료 코드 0, 539줄. selector guard
  probe 항목이 모두 true다(필드를 직접 설정하므로 스위치 삭제의 영향을 받지 않는다).
  `cache_executable=false`는 v0.0.207부터 있던 값이다.
* **core probe**: Linux x64 모두 통과. Win32는 `stack_bridge`만 실패(#8 기존).
* **변수 읽기**: `src/`·`include/`에 세 변수 문자열이 없다.
* **게임 실행 안 함**: 변수 없을 때 세 옵션의 값은 변경 전과 같은 `use_dynamic_backend`다.

---

# Work log: retire the guarded segment load, pop and read kill switches (issue #30)

**Done.** The loader's three reads of `REPIU_AOT_GUARDED_SEGMENT_POP/LOAD/READ` through
`ResolvePromotedToggle` became plain `use_dynamic_backend` assignments, with the promotion
history folded into one comment; the build-option and placement fields and the loader's
`enabled/sites` lines stay. The ARCHITECTURE pop and load sections now record the deletion, the
frontier's Task 384 record gained a line, the inventory's held item became a deletion, and the
TODO's deferred item became the remaining work (two undecided experiments, unreviewed
diagnostics, eleven recently promoted switches).

**Verification.** Win32 and Linux x64 Release builds with no errors. The full aot_probe chain on
`MASTER/PIU_1ST/PIU/PIU.EXE` exits 0 with 539 lines and every selector guard item true (those
probes set the fields directly); `cache_executable=false` dates from v0.0.207. Core probe: all
pass on Linux x64; on Win32 only `stack_bridge` fails (#8). No source reads the three variables.
No game run: with nothing set the three options keep their previous value,
`use_dynamic_backend`.
