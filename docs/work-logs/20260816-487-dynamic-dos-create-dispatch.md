# Dynamic DOS 파일 생성 디스패치 작업 로그

관련 문서: [설계](../design/20260816-487-dynamic-dos-create-dispatch.md),
[작업 지시](../work-orders/20260816-487-dynamic-dos-create-dispatch.md)

## 결과

`HandleTracedDosInterrupt21()`의 `AH=3Ch`를 공용 `HandleDosInterrupt21()`로
전달했습니다. 파일 생성 로직은 복제하지 않았으며 기존 Task 477의 VFS 구현이 계수,
반환값과 EIP를 계속 소유합니다.

`dos_file_create_probe`에는 실제 `CD 21` instruction과 `DISPATCH.TXT` guest path를
가진 runtime buffer를 추가했습니다. 이 probe는 기존 파일시스템 단위 검증에 더해
dynamic 추적 dispatcher 통합 경계를 직접 통과합니다.

## 검증

* `cmd /c scripts\build_win32_x86_release.bat repiu_aot_probe`: 성공
* `build\win32_x86_debug\Release\repiu_aot_probe.exe build\runtime_mounts\pumpit8\PIU\PIU.EXE`:
  종료 코드 0, `dos_file_create_traced_dispatch=true`, `dos_file_create_all=true`
* `cmd /c scripts\build_win32_x86_release.bat repiu`: 성공

빌드는 기존 C4819 코드 페이지 경고와 optional LibUSB 탐색 실패를 출력했지만 대상은
정상 생성되었습니다. live 게임 재실행은 이번 자동 검증 범위에 포함하지 않았으므로,
다음 실행에서 `ERRLOG.txt` 내용과 그 다음 execution frontier를 확인해야 합니다.

첫 probe 뒤 scratch root가 남아 close 경로를 재검토했습니다. `CloseDosFile()`이 read
stream만 reset하고 write stream cache는 유지하는 기존 결함을 확인했으며, 두 cache를
모두 해제하고 cleanup 성공을 probe 조건에 추가했습니다.

# Dynamic DOS File-Create Dispatch Work Log

Related documents: [design](../design/20260816-487-dynamic-dos-create-dispatch.md),
[work order](../work-orders/20260816-487-dynamic-dos-create-dispatch.md)

## Result

Traced `AH=3Ch` now delegates to the common `HandleDosInterrupt21()`. File-create
semantics were not duplicated; Task 477's VFS implementation remains the sole
owner of accounting, return state, and EIP advancement.

The first probe run left its scratch root behind. Review showed an existing
close-path defect: `CloseDosFile()` reset the read stream but retained the write
stream cache. Close now releases both caches, and cleanup is a probe condition.

`dos_file_create_probe` now constructs a runtime buffer with a real `CD 21`
instruction and `DISPATCH.TXT` guest path, directly covering the dynamic traced
dispatcher boundary in addition to the existing filesystem unit checks.

## Verification

* `cmd /c scripts\build_win32_x86_release.bat repiu_aot_probe`: passed
* `build\win32_x86_debug\Release\repiu_aot_probe.exe build\runtime_mounts\pumpit8\PIU\PIU.EXE`:
  exit code 0, `dos_file_create_traced_dispatch=true`,
  `dos_file_create_cleanup=true`, `dos_file_create_all=true`
* `cmd /c scripts\build_win32_x86_release.bat repiu`: passed

The build retained pre-existing C4819 code-page warnings and the optional
LibUSB discovery warning, but produced both targets. A live game rerun was not
part of this automated verification; the next run should inspect `ERRLOG.txt`
and the next execution frontier.
