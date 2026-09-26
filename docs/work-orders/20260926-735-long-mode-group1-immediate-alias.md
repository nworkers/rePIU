# Task 735: long mode에서 `82 /r ib`를 `80 /r ib`로 바꾸는 작업 지시

설계: [20260926-735](../design/20260926-735-long-mode-group1-immediate-alias.md)

## 한국어

1. `main`에서 작업 branch `work/20260926-735-pumpitea-aot-decode-verification`을 만든다.
2. 사용자 로그를 WSL Linux x64에서 재현한다.
3. decode 검증이 실패 항목을 기록하게 하고(`AotDecodeFailureSample`), 로더가 빌드 실패 시
   guest·방출 바이트와 함께 출력하게 한다.
4. 실패 항목을 특정하고, long-mode 분류기와 lowerer가 `82`를 `80` 표기로 판단·방출하게 한다.
5. `long_mode_compatibility` core probe에 `long_mode_group1_immediate_alias`를 추가한다.
6. Linux x64 core probe, pumpitea 실행, pumpit2a 회귀 실행, Win32 Debug 빌드로 검증한다.
7. 설계, 작업 로그, analysis, kb, `EXE_DESIGN.*`를 갱신하고 커밋한다.

## English

1. Create the task branch `work/20260926-735-pumpitea-aot-decode-verification` from `main`.
2. Reproduce the user's log on WSL Linux x64.
3. Make the decode check record its failing entries (`AotDecodeFailureSample`) and have the loader print
   them with guest and emitted bytes when the build fails.
4. Identify the failing entry, and make the long-mode classifier and lowerer judge and emit `82` as its
   `80` spelling.
5. Add `long_mode_group1_immediate_alias` to the `long_mode_compatibility` core probe.
6. Verify with the Linux x64 core probe, a pumpitea run, a pumpit2a regression run and a Win32 Debug
   build.
7. Update the design, work log, analysis, kb and `EXE_DESIGN.*`, then commit.
