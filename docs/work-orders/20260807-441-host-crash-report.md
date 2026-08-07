# Task 441 작업 지시 — 호스트 크래시 자체 보고

## 1. 왜

호스트 측 크래시가 **보이지 않았습니다.** 프로세스가 종료 코드만 남기고 사라져서,
Task 440은 teardown 폴트 하나를 두고 가설→수정→재빌드를 다섯 번 돌렸습니다. 이 기계에는
Windows 디버깅 도구가 없고 SDK는 dbghelp DLL만 제공합니다.

## 2. 범위

처리되지 않은 예외가 프로세스 최상단에 도달하면 **폴트 주소와 심볼화된 호스트 스택**을
stderr에 남깁니다. `main` 시작 시 1회 설치하며, 어차피 프로세스를 죽일 예외에서만
동작하므로 결함을 가릴 수 없습니다.

**건드리지 않을 것:** 게스트 폴트용 VEH, SEH 복구 경로, 종료 코드.

## 3. 구현 규칙

* `SetUnhandledExceptionFilter` 하나. 예외를 **삼키지 않습니다**(보고 후 프로세스 종료).
* 접근 위반은 **읽기/쓰기 구분과 대상 주소**까지 적습니다 — 대개 그것이 결함을 지목합니다.
* 심볼이 없으면 `module+offset`으로 떨어집니다. PDB 없이도 안전합니다.

## 4. 검증

의도적으로 크래시하는 실행에서 폴트 주소·스택·소스 줄이 출력됩니다.

---

# Task 441 Work Order — the loader reports its own crash

A host-side crash was invisible: the process vanished with an exit code, and Task 440 spent five
build-and-run rounds guessing at a teardown fault. The debugging tools are not installed here and
the SDK ships only the dbghelp DLLs, so the loader prints the faulting address and a symbolised
host stack from a single `SetUnhandledExceptionFilter` installed at the top of `main`. It fires
only on an exception that would have killed the process anyway, never swallows one, names the
access kind and target address for an access violation, and falls back to module and offset when
no PDB resolves. Verified by a run that crashes on purpose.
