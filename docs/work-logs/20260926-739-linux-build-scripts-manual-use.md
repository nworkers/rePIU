# Task 739 작업 로그 — Linux 빌드 스크립트를 수동 빌드에 쓸 수 있게 정비

설계: [20260926-739](../design/20260926-739-linux-build-scripts-manual-use.md)
작업 지시: [20260926-739](../work-orders/20260926-739-linux-build-scripts-manual-use.md)

## 요약

`scripts/build_linux_x64.sh`와 `scripts/build_linux_i386.sh`를 `--target` 없이 그대로 돌려도 끝까지
가도록 고쳤습니다. 막고 있던 것은 CMake 한 줄이었습니다. `repiu_exe`(static)의 Glide backend가 GL을
직접 부르는데 그 의존을 소비자에게 전하지 않아, 기본 타깃의 `repiu_instruction_census`가 두 아키텍처
모두에서, i386의 `repiu_core_probe`가 마지막 링크에서 실패했습니다. 이제 `repiu_exe`가 Linux에서 `GL`을
`PUBLIC`으로 링크합니다. 두 스크립트는 `--headless` 스위치를 양방향으로 명시하고, i386 스크립트는 x64와
같은 `--build-dir`과 구성 불일치 안내를 갖습니다.

## 확인한 것

* **기본 빌드 실패의 원인.** i386 트리를 `--target repiu --target repiu_core_probe`로 돌리자 `repiu`는
  빌드됐지만 `repiu_core_probe` 링크에서 `glGetError` 37회, `glTexCoord4f` 24회 등 GL 심볼이 미해결이었습니다.
  x64 트리에서는 core probe만 `GL`을 따로 링크하고 있어 `repiu_instruction_census`가 같은 이유로 실패했습니다
  (Task 735 로그의 "GL 심볼 링크 오류").
* **`--headless`의 실제 의미.** SDL3 `cmake/macros.cmake`에서 `SDL_UNIX_CONSOLE_BUILD`는 X11/Wayland
  개발 패키지가 없을 때 configure의 FATAL_ERROR를 막을 뿐입니다. `build/linux_x64_debug`·`build/linux_i386`은
  캐시에 `ON`이 남아 있어도 `SDL_X11`·`SDL_WAYLAND`·`SDL_PULSEAUDIO`가 모두 `ON`이고 창이 뜹니다. Task 738에서
  "headless 트리"라고 한 것은 정정했습니다(그 트리에 `repiu`가 없던 이유는 빌드된 적이 없어서였습니다).
* 스크립트는 스위치를 `--headless`일 때만 넘겨 캐시의 이전 값이 남았습니다. 양방향으로 넘기면 캐시가
  실제 요청을 반영합니다.

## 변경

| 파일 | 내용 |
|---|---|
| `CMakeLists.txt` | `if(UNIX AND NOT EMSCRIPTEN) target_link_libraries(repiu_exe PUBLIC GL)` |
| `scripts/build_linux_x64.sh` | console 스위치 양방향 명시, usage의 `--headless` 설명 정정 |
| `scripts/build_linux_i386.sh` | `--build-dir`, 구성 불일치 안내, console 스위치 양방향, 머리말 갱신 |
| `README.md`, `docs/guides/linux-shutdown-check.md` | `--headless` 서술 정정 |
| Task 738 작업 로그 | "headless 트리" 정정 |

## 검증

| 검증 | 결과 |
|---|---|
| `scripts/build_linux_i386.sh --config Release` (타깃 없음) | 성공. `repiu`·`repiu_core_probe`·`repiu_instruction_census`·`repiu_launcher` 생성, 캐시 `SDL_UNIX_CONSOLE_BUILD=OFF` |
| i386 core probe | 31/31, `build_identity … label="Linux/x86 Release"` |
| `cmake --build build/linux_x64_debug` (타깃 없음) | 성공, `repiu_instruction_census` 19 MB 링크 |
| `scripts/build_linux_x64.sh --config Release --build-dir build/linux_x64_release` | 성공, 캐시 `OFF`, core probe 33/33 `Linux/x64 Release` |
| i386 스크립트 불일치 안내 | Release 트리에 `--config Debug`를 주면 `--build-dir` 안내를 내고 종료 |

Win32 빌드는 하지 않았습니다. CMake 변경은 `UNIX AND NOT EMSCRIPTEN` 안에만 있습니다.

## 남은 것

* `docs/guides/linux-engine-port-measurement.md`는 `--headless --target repiu_core_probe`로 트리를 만들라고
  합니다. 스위치의 의미가 바뀌지는 않았으므로 그대로 두었습니다.
* 웹(wasm) 빌드는 확인하지 않았습니다.

---

# English

# Task 739 work log — the Linux build scripts for manual builds

Design: [20260926-739](../design/20260926-739-linux-build-scripts-manual-use.md)
Work order: [20260926-739](../work-orders/20260926-739-linux-build-scripts-manual-use.md)

## Summary

`scripts/build_linux_x64.sh` and `scripts/build_linux_i386.sh` now run to the end with no `--target`.
One CMake line stood in the way: the Glide backend inside `repiu_exe` (static) calls GL directly and
the library did not pass that dependency on, so the default targets' `repiu_instruction_census` failed
its last link on both architectures and the i386 `repiu_core_probe` failed too. `repiu_exe` now links
`GL` `PUBLIC` on Linux. Both scripts pass the `--headless` switch both ways, and the i386 script gained
the x64 script's `--build-dir` and configuration-mismatch guard.

## Findings

* **Why the default build failed.** Building the i386 tree with `--target repiu --target
  repiu_core_probe` produced `repiu` but the core probe's link left `glGetError` (37), `glTexCoord4f`
  (24) and other GL symbols unresolved; in the x64 tree only the core probe linked `GL` itself, so
  `repiu_instruction_census` failed the same way (the "GL symbol link error" in the Task 735 log).
* **What `--headless` really does.** In SDL3's `cmake/macros.cmake`, `SDL_UNIX_CONSOLE_BUILD` only
  suppresses the configure FATAL_ERROR when the X11/Wayland development packages are missing.
  `build/linux_x64_debug` and `build/linux_i386` hold `ON` in their caches yet have `SDL_X11`,
  `SDL_WAYLAND` and `SDL_PULSEAUDIO` on and open windows. Task 738's "headless tree" was corrected: that
  tree had no `repiu` because it had never been built.
* The scripts passed the switch only with `--headless`, so the cache kept a previous value; passing it
  both ways makes the cache reflect the request.

## Changes

`CMakeLists.txt`: `if(UNIX AND NOT EMSCRIPTEN) target_link_libraries(repiu_exe PUBLIC GL)`.
`scripts/build_linux_x64.sh`: the console switch passed both ways, the usage's `--headless` wording
corrected. `scripts/build_linux_i386.sh`: `--build-dir`, the mismatch guard, the console switch both
ways, an up-to-date header. `README.md` and `docs/guides/linux-shutdown-check.md`: `--headless` wording.
The Task 738 work log: the "headless tree" correction.

## Verification

`scripts/build_linux_i386.sh --config Release` with no target succeeds, producing `repiu`,
`repiu_core_probe`, `repiu_instruction_census` and `repiu_launcher`, with `SDL_UNIX_CONSOLE_BUILD=OFF`
in the cache; the i386 core probe passes 31/31 with `label="Linux/x86 Release"`. `cmake --build
build/linux_x64_debug` with no target succeeds and links `repiu_instruction_census` (19 MB).
`scripts/build_linux_x64.sh --config Release --build-dir build/linux_x64_release` succeeds with the
cache at `OFF` and the core probe 33/33 `Linux/x64 Release`. Giving the i386 script `--config Debug` on
the Release tree prints the `--build-dir` guidance and exits. No Win32 build was made: the CMake change
is inside `UNIX AND NOT EMSCRIPTEN`.

## What remains

* `docs/guides/linux-engine-port-measurement.md` still says to make its tree with `--headless --target
  repiu_core_probe`; the switch's meaning did not change, so it was left as is.
* The web (wasm) build was not checked.
