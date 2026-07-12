# 공용 native basic-block fast path 작업 지시

1. Zydis 기반 straight-line block analyzer와 cache를 구현합니다.
2. 기존 hardware breakpoint fast path 상태에 function/block mode와 별도 계수를 추가합니다.
3. single-step HLE 처리 후 function fast path, basic-block fast path 순서로 진입합니다.
4. intermediate exception은 fail closed하고 해당 block을 reject합니다.
5. live/final telemetry에 block entry/exit/cancel을 추가합니다.
6. Win32 x86 build, OpenWatcom baseline, pumpit1 단계별 실행으로 검증합니다.
7. analysis/architecture/work log를 갱신하고 커밋합니다.

# Generic Native Basic-Block Fast Path Work Order

Implement and cache Zydis straight-line block analysis, integrate function/block modes with fail-closed hardware-breakpoint execution, add telemetry, validate Win32 x86, OpenWatcom, and staged pumpit1 performance, update documentation, and commit the task.
