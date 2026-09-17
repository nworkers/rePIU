# Task 693 — 공용 LINEXE far-jump dispatch

공용 guest HLE dispatcher의 `EA` 분기에서 기존
`HandleLinexeFarTransferBoundary`를 일반 far jump보다 먼저 호출하도록
연결했습니다. 기존 LINEXE matcher와 service semantics는 재사용했습니다.
bridge frame을 읽을 수 없으면 저장된 이전 frame을 소비하지 않고 즉시
거부하며, LINEXE가 처리하지 않은 export는 기존 generic far-jump 경로로
넘어갑니다.

공용 dispatch probe는 문서화된 `LINEXE_LOADMODULE` export와 미지원 export,
unreadable frame을 각각 확인했습니다.

```text
linexe_shared_dispatch=1,fallback=1,bad_frame=1
general_stack_push=true,...,boundary_epilogue=true
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

WSL Ubuntu 24.04에서 최신 `repiu_core_probe`와 `repiu`를 재빌드했고,
core probe 27개 그룹이 모두 통과했습니다. Windows-mounted 경로에서
최상위 Makefile의 timestamp 판단이 오래된 object를 유지하여, 변경된
probe object를 CMake 생성 `build.make`에서 강제 재컴파일한 후 두 실행
파일을 링크했습니다. 기존 `execution_trampoline.cpp`의 extern 경고 외
빌드 오류는 없습니다.

`pumpit2a` 동적 smoke에서는 다음 selector binding을 확인했습니다.

```text
[loader] Win32 relocated selector binding: selector=0x002C object=3 base=0x01100000 limit=0x00000047
[repiu-shutdown] reason=timeout attempts=15 answered=1 recovered=1 stopped=1 failure=0 eip=0x200633EA gate=0 frames=0 span_ms=0
```

이번 실행은 `0x010...` 동적 code만 요청했고 `0x01100022` LINEXE export
요청이나 `linexe_shared_dispatch` live trace까지는 도달하지 않았습니다.
따라서 공용 dispatch의 실제 게임 경로 진입과 정상 게임 종료는 아직
미확정입니다. 이 smoke에서 coredump failure가 재현되지는 않았지만,
timeout shutdown은 성공 실행의 증거가 아닙니다.

다음 작업은 mode16 SS-relative far-return frame을 읽어 bare `RETF`를
검증하고, object 3의 실제 LINEXE export 경계까지 도달하는 실행 경로를
확인하는 것입니다.

## English

The shared guest HLE dispatcher now tries the existing
`HandleLinexeFarTransferBoundary` from the `EA` branch before generic far
jump handling. Existing LINEXE matching and service semantics are reused.
An unreadable bridge frame is rejected without consuming a previously saved
frame, while an export declined by LINEXE continues through generic far-jump
handling.

The shared-dispatch probe covers the documented `LINEXE_LOADMODULE` export, an
unsupported export, and an unreadable frame:

```text
linexe_shared_dispatch=1,fallback=1,bad_frame=1
general_stack_push=true,...,boundary_epilogue=true
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

The latest `repiu_core_probe` and `repiu` were rebuilt on WSL Ubuntu 24.04,
and all 27 core-probe groups passed. Because the Windows-mounted tree caused
the top-level Makefile timestamp check to retain stale objects, the changed
probe object was force-compiled through CMake's generated `build.make` before
linking both executables. The only build diagnostic was the existing extern
declaration warning in `execution_trampoline.cpp`.

The `pumpit2a` dynamic smoke confirmed the selector binding:

```text
[loader] Win32 relocated selector binding: selector=0x002C object=3 base=0x01100000 limit=0x00000047
[repiu-shutdown] reason=timeout attempts=15 answered=1 recovered=1 stopped=1 failure=0 eip=0x200633EA gate=0 frames=0 span_ms=0
```

This run requested only `0x010...` dynamic code and did not reach the
`0x01100022` LINEXE export request or a live `linexe_shared_dispatch` trace.
The shared dispatch's real game-path entry and normal game termination remain
unverified. No coredump failure was reproduced in this smoke, but timeout
shutdown is not evidence of successful execution.

Next, validate the mode16 SS-relative far-return frame and bare `RETF`, then
find an execution path that reaches the actual object-3 LINEXE export boundary.
