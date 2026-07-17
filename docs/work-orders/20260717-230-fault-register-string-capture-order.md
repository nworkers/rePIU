# 작업 지시: fault 시점 레지스터 문자열 캡처 진단
# Work Order: Fault-time register string capture diagnostic

관련 frontier: `docs/analysis/current-execution-frontier.md` Task 229 절
관련 메모리: task229-bga-texture-extension-frontier

## 1. 목표 / Goal

Task 229 frontier(`0x030F4A98` stricmp null-deref)의 근인을 규명하기 위해, fault 시점
각 범용 레지스터(EAX/EBX/ECX/EDX/ESI/EDI)가 가리키는 게스트 메모리의 ASCII 문자열을
캡처·보고하는 진단을 추가한다. 특히 `ESI`(파싱 대상 소스 파일명, callee-saved)의 실제
문자열을 확인해 "확장자 없는 파일명"이 무엇인지 밝힌다.

이 진단은 향후 널 포인터/문자열 관련 frontier에도 재사용 가능한 범용 계측이다.

## 2. 변경 대상 / Files

* `include/repiu/platform/win32/execution_trampoline.h`
  - attempt 관찰 구조체에 `exception_register_strings[6][32]` +
    `exception_register_string_valid_mask` 추가.
* `src/platform/win32/execution_trampoline.cpp`
  - 동일 필드를 ThreadContext에 추가.
  - `CaptureException`에서 EAX/EBX/ECX/EDX/ESI/EDI 각각을 시작 주소로 최대 32바이트
    `ReadProcessMemory`로 캡처(첫 바이트 읽히면 valid 비트 설정).
  - context→attempt 미러링에 필드 복사 추가.
* `src/host/win32/main.cpp`
  - 리포트에 각 레지스터 문자열을 출력(비인쇄 문자는 표시용으로 '.'로 치환, 원본
    바이트도 hex로 병기하면 유용).

## 3. 검증 / Verification

* 빌드(`build/win32_x86_dpmi`, loader).
* `aot-dynamic pumpit1` 구동으로 Task 229 fault 재현 시 리포트에서 `ESI` 문자열
  (소스 파일명)을 확인. 근인(확장자 손실 지점) 규명으로 이어간다.

---

**English summary.** Add a reusable fault-time diagnostic that captures up to 32 ASCII bytes
at each GPR (EAX/EBX/ECX/EDX/ESI/EDI) in `CaptureException` and prints them in the report, to
reveal the source filename at `ESI` for the Task 229 frontier (`0x030F4A98` stricmp null-deref)
and to help future null-pointer/string frontiers. Touch points: the attempt struct
(`execution_trampoline.h`), `ThreadContext` + `CaptureException` + the context→attempt mirror
(`execution_trampoline.cpp`), and the report (`main.cpp`). Verify by an aot-dynamic `pumpit1`
run that reproduces the fault and shows the `ESI` string.
