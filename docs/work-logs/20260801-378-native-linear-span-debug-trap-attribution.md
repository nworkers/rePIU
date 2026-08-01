# 작업 로그: Native linear span #DB 취소 원인 계측 / Work log: native linear-span #DB cancellation attribution

## 결과

- 예상 밖 native linear span `EXCEPTION_SINGLE_STEP` 취소를 DR6의 Dr0/Dr1/Dr2/Dr3/BS(TF)/기타 원인으로 상호 배타적으로 집계하도록 추가했습니다.
- 각 원인에 대해 첫 취소 EIP를 보존하고, live snapshot 및 종료 로그에 노출했습니다.
- 예상된 Dr0 경계와 write-fault 취소의 기존 처리, TF/Dr 복원, fallback 제어 흐름은 바꾸지 않았습니다.
- `git diff --check`를 통과했습니다.
- `cmd /c scripts\build_win32_x86_release.bat`는 전체 재구성·광범위 재빌드가 120초 실행 제한에 걸려 완료되지 않았습니다. 제한 전 새 snapshot 매핑 소스는 컴파일 단계까지 통과했고, native span 및 종료 로그 소스까지는 도달하지 못했습니다.
- runtime music-select 캡처는 후속으로 필요합니다. 새 로그의 `Win32 native linear span #DB cancel...` 및 `#DB first eip...` 행으로 다음 수정 후보를 결정합니다.

## English

- Added mutually exclusive attribution for unexpected native linear-span `EXCEPTION_SINGLE_STEP` cancellations: DR6 Dr0/Dr1/Dr2/Dr3/BS (TF)/other.
- Preserved the first cancellation EIP for each cause and exposed the values through the live snapshot and final log.
- Did not change expected Dr0-boundary or write-fault cancellation handling, TF/Dr restoration, or fallback control flow.
- `git diff --check` passed.
- `cmd /c scripts\build_win32_x86_release.bat` did not complete because its full reconfigure/broad rebuild reached the 120-second execution limit. Before the limit, the new snapshot-mapping source reached compilation successfully; native-span and final-log sources were not reached.
- A follow-up runtime music-select capture is required. The new `Win32 native linear span #DB cancel...` and `#DB first eip...` lines determine the next fix candidate.