# 정적 TargetProfile 작업 로그

## 결과

DOS/4GW 공용 로더 방향에 맞춰 정적 target profile 등록 구조를 추가했다.

`TargetProfile`은 target id, 표시 이름, 실행 파일 경로, 작업 디렉터리, 자산 루트, 실행 파일 포맷 힌트, HLE 프로파일 id를 가진다.

`TargetRegistry`는 내장 profile 목록과 id 조회 함수를 제공한다.

초기 내장 profile로 `piu_1st`를 등록했다.

`repiu_exe_analyzer`는 인자가 없을 때 `piu_1st` profile의 실행 파일 경로를 기본값으로 사용한다.

이번 단계에서는 동적 플러그인 시스템이나 HLE override 실행을 추가하지 않았다.

## 변경 파일

* `CMakeLists.txt`
* `include/repiu/target/target_profile.h`
* `src/target/target_profile.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/PROJECT_CHARTER.md`
* `docs/design/20260708-011-static-target-profile.md`
* `docs/work-orders/20260708-011-static-target-profile.md`

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

인자 없는 분석 도구 실행:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

주요 출력:

```text
Target: piu_1st
Target name: PIU 1st
Format hint: DOS4GW_LE
Working directory: MASTER/PIU_1ST
Asset root: MASTER/PIU_1ST
HLE profile: piu_common
Path: MASTER/PIU_1ST/PIU.EXE
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

명시 경로 분석 도구 실행:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

주요 출력:

```text
Path: MASTER\PIU_1ST\PIU.EXE
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

## 다음 단계

다음 단계에서는 `HleProfile`의 최소 구조를 추가하거나, 현재 `src/exe/`의 parser/image/relocation 함수를 `Dos4gwExecutableLoader` 단위로 묶는 설계를 진행한다.

## Result

Added a static target profile registration structure aligned with the shared DOS/4GW loader direction.

`TargetProfile` contains the target id, display name, executable path, working directory, asset root, executable format hint, and HLE profile id.

`TargetRegistry` provides the built-in profile list and id lookup.

Registered `piu_1st` as the initial built-in profile.

`repiu_exe_analyzer` uses the executable path from the `piu_1st` profile by default when no argument is provided.

This step does not add a dynamic plugin system or execute HLE overrides.

## Changed Files

* `CMakeLists.txt`
* `include/repiu/target/target_profile.h`
* `src/target/target_profile.cpp`
* `src/tools/exe_analyzer/main.cpp`
* `ARCHITECTURE.md`
* `docs/PROJECT_CHARTER.md`
* `docs/design/20260708-011-static-target-profile.md`
* `docs/work-orders/20260708-011-static-target-profile.md`

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

Analyzer run without arguments:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe
```

Key output:

```text
Target: piu_1st
Target name: PIU 1st
Format hint: DOS4GW_LE
Working directory: MASTER/PIU_1ST
Asset root: MASTER/PIU_1ST
HLE profile: piu_common
Path: MASTER/PIU_1ST/PIU.EXE
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

Analyzer run with an explicit path:

```text
build\vs2022_debug\Debug\repiu_exe_analyzer.exe MASTER\PIU_1ST\PIU.EXE
```

Key output:

```text
Path: MASTER\PIU_1ST\PIU.EXE
LE relocation dry run: valid
LE applied relocations: 14637
LE failed relocations: 0
LE skipped relocations: 10
```

## Next Step

The next step should either add the minimum `HleProfile` structure or design a `Dos4gwExecutableLoader` unit around the current parser/image/relocation functions under `src/exe/`.
