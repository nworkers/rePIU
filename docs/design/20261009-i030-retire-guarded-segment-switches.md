# 설계: guarded segment load·pop·read 끄기 스위치 삭제 (issue #30)

근거: `docs/analysis/environment-toggle-inventory.md`의 보류 항목. 1번 묶음(#20)과 같은 형태다.

## 배경

guarded segment pop(Task 291), read(Task 384), load(Task 390)는 `dynamic` 백엔드에서 기본
켜기로 승격된 지 오래됐지만, v0.0.206(#18)에서 세 슬롯의 내부를 shadow 진실원 방식으로
바꾸면서 `REPIU_AOT_GUARDED_SEGMENT_*=0`이 새 슬롯 문제를 가려낼 유일한 비상 수단이 됐다.
그래서 "v0.0.206 이후 한 릴리스 동안 새 슬롯 회귀가 없으면"을 재개 조건으로 두고 보류했다.
v0.0.207~v0.0.210 네 릴리스 동안 보고된 회귀가 없다.

## 처리

* loader의 세 줄을 `ResolvePromotedToggle(getenv(...))`에서 `use_dynamic_backend`로 바꾼다.
* 빌드 옵션 필드 `enable_guarded_segment_pop/read/load`와 placement 필드는 남긴다. legacy
  백엔드는 계속 꺼진 값을 쓰고, selector guard·long mode probe가 켜진 이미지와 꺼진 대조
  이미지를 만들 때 필드를 직접 설정한다(#20의 AOT 빌드 토글과 같은 이유).
* loader 로그의 `AOT guarded segment-pop/load/read enabled …` 줄은 그대로 둔다. site 수를 함께
  보고하므로 진단 가치가 남는다.
* 문서: ARCHITECTURE의 pop·load 절, frontier의 Task 384 승격 기록, 환경 변수 목록, TODO.

변수 없이 실행했을 때의 동작과 방출 코드는 바뀌지 않는다.

## 검증

Win32·Linux x64 Release 빌드, Win32 aot_probe 전체 체인(selector guard·long mode probe 포함),
core probe, 변수 읽기 `grep`. 기본 실행 경로의 값이 같으므로 게임 실행은 하지 않는다.

---

# Design: retire the guarded segment load, pop and read kill switches (issue #30)

Source: the deferred item in `docs/analysis/environment-toggle-inventory.md`; the same shape as
group 1 (#20).

**Background.** Guarded segment pop (Task 291), read (Task 384) and load (Task 390) were promoted
to default-on for the `dynamic` backend long ago, but v0.0.206 (#18) moved all three slots onto
the shadow-authoritative design, leaving `REPIU_AOT_GUARDED_SEGMENT_*=0` as the only escape
hatch for the new slots. The switches were held until one release after v0.0.206 passed without
a regression against the new slots; none was reported through v0.0.207 to v0.0.210.

**Change.** The loader's three `ResolvePromotedToggle(getenv(...))` reads become
`use_dynamic_backend`. The build-option and placement fields stay: the legacy backend keeps the
off value, and the selector guard and long mode probes set the fields directly to build the
enabled image and the disabled control image (the same reason #20 kept its AOT build fields). The
loader's `AOT guarded segment-pop/load/read enabled …` lines stay, since they report site counts.
Docs: the ARCHITECTURE pop and load sections, the frontier's Task 384 promotion record, the
inventory and the TODO. Execution and emitted code with nothing set do not change.

**Verification.** Win32 and Linux x64 Release builds, the full Win32 aot_probe chain (including
the selector guard and long mode probes), the core probe, and `grep` for reads. No game run: the
default path's values are unchanged.
