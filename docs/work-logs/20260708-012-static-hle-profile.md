# 정적 HLE Profile 작업 로그

## 결과

target profile이 참조하는 `hle_profile_id`를 해석할 수 있도록 정적 HLE profile registry를 추가했다.

`HleProfile`은 profile id, 표시 이름, 설명, 필요한 HLE 서비스 목록을 가진다.

초기 내장 profile로 `piu_common`을 등록했다.

`repiu_exe_analyzer`는 target profile의 `hle_profile_id`를 조회해 HLE profile 이름과 service 목록을 출력한다.

이번 단계에서는 실제 DOS/DPMI/HW HLE 동작이나 hook 실행을 추가하지 않았다.

## 변경 파일

* `CMakeLists.txt`
* `include/repiu/hle/hle_profile.h`
* `src/hle/hle_profile.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/PROJECT_CHARTER.md`
* `docs/design/20260708-012-static-hle-profile.md`
* `docs/work-orders/20260708-012-static-hle-profile.md`

## 검증

Debug 빌드:

```text
cmake --build build\vs2022_debug --config Debug
```

결과:

```text
repiu_exe.lib 빌드 성공
repiu_exe_analyzer.exe 빌드 성공
```

분석 도구 실행:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

주요 출력:

```text
HLE profile: piu_common
HLE profile name: PIU common HLE
HLE services: DOS file DOS memory DPMI timer input video audio
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

기존 실행 파일 분석과 relocation dry-run 결과가 유지되었다.

## 다음 단계

다음 단계에서는 현재 parser/image/relocation 함수를 `Dos4gwExecutableLoader` 단위로 묶어, analyzer와 향후 runtime이 같은 로더 결과 구조를 공유하도록 설계한다.

## Result

Added a static HLE profile registry that resolves the `hle_profile_id` referenced by target profiles.

`HleProfile` contains the profile id, display name, description, and required HLE service list.

Registered `piu_common` as the initial built-in profile.

`repiu_exe_analyzer` resolves the target profile's `hle_profile_id` and prints the HLE profile name and service list.

This step does not add actual DOS/DPMI/HW HLE behavior or hook execution.

## Changed Files

* `CMakeLists.txt`
* `include/repiu/hle/hle_profile.h`
* `src/hle/hle_profile.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/PROJECT_CHARTER.md`
* `docs/design/20260708-012-static-hle-profile.md`
* `docs/work-orders/20260708-012-static-hle-profile.md`

## Verification

Debug build:

```text
cmake --build build\vs2022_debug --config Debug
```

Result:

```text
repiu_exe.lib build succeeded
repiu_exe_analyzer.exe build succeeded
```

Analyzer run:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

Key output:

```text
HLE profile: piu_common
HLE profile name: PIU common HLE
HLE services: DOS file DOS memory DPMI timer input video audio
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

The existing executable analysis and relocation dry-run results are preserved.

## Next Step

The next step should group the current parser/image/relocation functions into a `Dos4gwExecutableLoader` unit so the analyzer and future runtime can share the same loader result structure.
