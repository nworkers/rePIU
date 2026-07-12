# OpenWatcom sample baseline 0.0.34 갱신 작업 지시

1. 로컬 tool/sample manifest와 기존 baseline 상태를 확인합니다.
2. 필요하면 현재 Win32 loader와 sample manifest를 다시 빌드합니다.
3. `-CompareBaseline`으로 regression/new pass를 확인합니다.
4. 결과가 유효하면 `-UpdateBaseline`으로 baseline과 history JSON/HTML을 갱신합니다.
5. 갱신 후 다시 baseline 비교를 실행하고 잔류 process를 확인합니다.
6. 작업 로그와 커밋을 남깁니다.

# OpenWatcom Sample Baseline 0.0.34 Refresh Work Order

Inspect the local suite, rebuild when needed, compare against the current baseline, update the baseline and JSON/HTML history, compare once more, check for residual processes, document the result, and commit it.
