# AOT retire target inline-cache coherence work order

## 목표 / Goal

retired guest page를 target으로 가진 AOT inline-cache hit를 miss로 되돌려 stale native
target 진입을 막습니다.

Invalidate AOT inline-cache hits targeting a retired guest page so they cannot enter a
stale native target.

## 범위 / Scope

- `aot_page_coherence_win32.cpp`의 retire 처리
- `aot_probe`의 최소 coherence 회귀 검증
- Win32 x86 build 및 180초 동적 실행 관찰

## 완료 기준 / Done criteria

- retire 뒤 해당 guest page target의 cache guard가 miss 상태입니다.
- `aot_probe`와 Win32 x86 Debug build가 성공합니다.
- 장시간 실행의 결과와 남은 frontier를 작업 로그에 남깁니다.

- After retirement, guards for targets in that guest page are in miss state.
- `aot_probe` and the Win32 x86 Debug build succeed.
- The long-run result and remaining frontier are recorded in the work log.
