# Glide Hints 경계 작업 지시 / Glide Hints Boundary Work Order

## 목표 / Goal

관측된 `_GRHINTS@8`를 원본 stdcall ABI로 처리하고, 장기 `aot-dynamic` 실행에서 다음 frontier를 기록합니다.

Handle observed `_GRHINTS@8` with its original stdcall ABI and record the next frontier in a long `aot-dynamic` run.

## 검증 / Verification

1. Win32 x86 Debug 전체 빌드.
2. supervisor 180000 ms 실행.
3. 기존 `_GRHINTS@8` access violation이 사라지고 다음 gate 또는 실행 상태가 기록되는지 확인.

1. Full Win32 x86 Debug build.
2. 180000 ms supervisor run.
3. Confirm the former `_GRHINTS@8` access violation is gone and record the next gate or execution state.
4. If a color-combine equation is rejected solely as unsupported by GLSL, retain logical state and verify guest progress continues.
