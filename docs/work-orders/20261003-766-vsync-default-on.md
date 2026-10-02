# 작업 지시 766: vsync 기본값 켜기

설계: [20261003-766-vsync-default-on.md](../design/20261003-766-vsync-default-on.md)

## 작업 항목

1. `include/repiu/engine/glide_swap_interval_policy.h`, `src/engine/glide_swap_interval_policy.cpp`
   * `kDefaultGlideSwapInterval = 1` 추가.
   * `ResolveGlideSwapInterval(const char*, std::int32_t*)` 추가, `TryReadGlideSwapIntervalOverride`를
     `ReadGlideSwapInterval(std::int32_t*)`로 대체.
2. `src/engine/glide_opengl_backend.cpp`: interval을 항상 요청하고 Task 745 거부 대응을 그대로 적용.
3. `src/host/loader/main.cpp`: 페이싱 보고 줄을 항상 출력.
4. `src/launcher/launcher_ui.cpp`: 저장값이 없으면 체크박스 켜짐.
5. `src/tools/aot_probe/glide_swap_interval_policy_probe.cpp`: 기본값 해석 검사 추가.
6. 문서: `README.md` vsync 문단, `ARCHITECTURE.md` swap 절, 작업 로그.

## 완료 조건

* Linux x64 빌드 성공, `aot_probe`의 `glide_swap_interval_all=true`.
* Wayland에서 환경 변수 없이 실행하면 최종 보고가 `false/1/true/1`.

---

# Work Order 766: Vertical sync on by default

Design: [20261003-766-vsync-default-on.md](../design/20261003-766-vsync-default-on.md)

## Tasks

1. `glide_swap_interval_policy`: add `kDefaultGlideSwapInterval = 1` and
   `ResolveGlideSwapInterval(const char*, std::int32_t*)`; replace
   `TryReadGlideSwapIntervalOverride` with `ReadGlideSwapInterval(std::int32_t*)`.
2. `glide_opengl_backend.cpp`: always request the interval, keeping Task 745's refusal handling.
3. `main.cpp`: print the pacing report lines always.
4. `launcher_ui.cpp`: the checkbox reads as on when nothing is stored.
5. The `glide_swap_interval` probe: check the default resolution.
6. Documents: the README vsync paragraph, the ARCHITECTURE swap section, the work log.

## Done when

* The Linux x64 build succeeds and `aot_probe` prints `glide_swap_interval_all=true`.
* Under Wayland without the variable the final report reads `false/1/true/1`.
