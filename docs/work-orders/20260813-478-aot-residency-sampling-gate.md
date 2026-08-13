# AOT residency 표본화 게이트 작업 지시

1. `include/repiu/platform/win32/aot_residency_sample.h`와
   `src/platform/win32/telemetry/aot_residency_sample.cpp`를 추가하고,
   `aot_runtime_dispatch.cpp`의 `AccumulateAotResidency` 본문을 그대로 옮깁니다.
   함수 이름과 서명은 유지합니다.
2. 새 파일에 `ResolveAotResidencySampleEnabled(const char*)`와
   `AotResidencySampleEnabled()`를 두고 `ResolveOptInToggle`로 해석합니다. 환경
   변수 이름은 `REPIU_AOT_RESIDENCY_SAMPLE`입니다.
3. `AccumulateAotResidency` 가장 앞에서 게이트를 검사하고, OFF면 `ExecutionTimeScope`
   생성과 decoder 초기화 이전에 반환합니다.
4. `ZydisDecoderInit`을 함수 지역 `static` 1회 초기화로 바꿉니다. 초기화 실패는
   기존과 같이 조용히 반환합니다.
5. `aot_runtime_dispatch.cpp`에서 옮긴 본문과 이제 쓰이지 않는 include를 제거하고,
   `aot_runtime_dispatch.h`의 선언도 새 header로 옮깁니다.
6. 호출 지점 9곳의 include를 새 header로 바꿉니다 — `aot_dbt_dispatch.cpp`,
   `aot_dbt_hle_dispatch.cpp`(2곳), `aot_dbt_glide_gate_dispatch.cpp`,
   `aot_runtime_dispatch.cpp`(5곳). 호출 형태는 바꾸지 않습니다.
7. `main.cpp`의 residency 요약 줄에 게이트 상태를 넣어
   `Win32 AOT residency enabled/total/samples/avg/max/coverage%`로 바꿉니다.
8. `CMakeLists.txt`에 새 소스를 등록합니다.
9. Win32 x86 Debug 빌드를 수행하고 `aot_probe`를 실행합니다.
10. 설계와 `docs/analysis/current-execution-frontier.md`를 갱신하고 작업 로그를
    남긴 뒤 하나의 작업 커밋으로 정리합니다.
11. 사용자 구동 A/B로 `kAotResidency` 소멸과 fps 변화를 확인합니다. 프레임당 작업량이
    3% 이내로 일치할 때만 fps 비교를 인정합니다.

# AOT Residency Sampling Gate Work Order

1. Add `include/repiu/platform/win32/aot_residency_sample.h` and
   `src/platform/win32/telemetry/aot_residency_sample.cpp`, moving the body of
   `AccumulateAotResidency` from `aot_runtime_dispatch.cpp` unchanged. Keep the
   function name and signature.
2. Put `ResolveAotResidencySampleEnabled(const char*)` and
   `AotResidencySampleEnabled()` in the new file, resolved through
   `ResolveOptInToggle`. The environment variable is
   `REPIU_AOT_RESIDENCY_SAMPLE`.
3. Check the gate at the very top of `AccumulateAotResidency` and return before
   constructing the `ExecutionTimeScope` or initializing the decoder when it is
   off.
4. Promote `ZydisDecoderInit` to a one-time function-local `static`. An
   initialization failure still returns silently, as before.
5. Remove the moved body and the now-unused includes from
   `aot_runtime_dispatch.cpp`, and move the declaration out of
   `aot_runtime_dispatch.h` into the new header.
6. Point the nine call sites at the new header — `aot_dbt_dispatch.cpp`,
   `aot_dbt_hle_dispatch.cpp` (two), `aot_dbt_glide_gate_dispatch.cpp`, and
   `aot_runtime_dispatch.cpp` (five). Leave the call form unchanged.
7. Extend the `main.cpp` residency summary to carry the gate state, as
   `Win32 AOT residency enabled/total/samples/avg/max/coverage%`.
8. Register the new source in `CMakeLists.txt`.
9. Run the Win32 x86 Debug build and execute `aot_probe`.
10. Update the design and `docs/analysis/current-execution-frontier.md`, leave a
    work log, and land one task commit.
11. Confirm from the user's A/B run that `kAotResidency` disappears and how fps
    moves. The fps comparison counts only when per-frame work agrees within 3%.
