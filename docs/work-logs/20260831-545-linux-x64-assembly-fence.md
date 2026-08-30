# 20260831-545 Linux x64 assembly 경계 작업 로그

## 한국어

### 결과

Linux x64 구성에서 i386 전용 `aot_dbt_dispatch_thunks.S`와
`guest_stack_switch.S`를 수집하지 않도록 빌드 경계를 추가했습니다. core probe의
`stack_bridge`와 `guest_stack_switch`도 같은 이유로 x64에서 제외하고, 실행 결과에
skipped 항목으로 남기도록 했습니다.

Linux i386에서는 기존 assembly와 두 probe가 그대로 유지됩니다. x64 guest entry는
Task 544의 fail-closed 동작을 유지하므로 이번 변경은 실행 지원을 가장하지 않습니다.

### 검증

`git diff --check`는 통과했습니다. 기존 WSL x64 build tree를 사용한 재빌드는
Windows 세션에서 경로 불일치가 발생했고, WSL로 재시도한 명령은
`Wsl/Service/CreateInstance/E_ACCESSDENIED`로 거부되었습니다. 사용량 제한으로
권한 상승 재시도도 허용되지 않았습니다.

대신 build cache를 정적으로 확인하여 x64 구성은 `CMAKE_*_SIZEOF_DATA_PTR=8`,
i386 구성은 `CMAKE_*_SIZEOF_DATA_PTR=4`임을 확인했습니다. 따라서 CMake 조건은
x64에서 두 i386 assembly와 두 assembly probe를 제외하고, i386에서는 기존 입력을
유지하는 형태입니다. 실제 `cmake --build` 결과와 core probe 출력은 다음 Linux
검증 세션에서 보완해야 합니다.

## English

### Result

Added a build boundary so Linux x64 does not collect the i386-only
`aot_dbt_dispatch_thunks.S` and `guest_stack_switch.S` files. The `stack_bridge` and
`guest_stack_switch` core probes are excluded for the same reason and remain visible as
skipped items in probe output.

Linux i386 retains the existing assembly and both probes. The x64 guest entry keeps
Task 544's fail-closed behavior, so this change does not pretend to provide execution
support.

### Verification

`git diff --check` passed. Rebuilding the existing WSL x64 tree from a Windows
session failed because its cache uses a different path, and the WSL retry was rejected
with `Wsl/Service/CreateInstance/E_ACCESSDENIED`. The elevated retry was unavailable
because of the session usage limit.

Static build-cache checks confirmed `CMAKE_*_SIZEOF_DATA_PTR=8` for x64 and `4` for
i386. The CMake condition therefore excludes the two i386 assembly units and their two
assembly-dependent probes on x64 while retaining them on i386. The concrete build and
core-probe output remain to be completed in the next Linux verification session.
