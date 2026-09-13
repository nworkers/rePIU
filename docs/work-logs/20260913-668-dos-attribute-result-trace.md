# Task 668 작업 로그: DOS AH=43h 파일 속성 결과 추적

## 한국어

### 작업 목적

Task 667에서 확인한 zero writer의 앞선 DOS/HLE 결과를 공통 경로에서
확인합니다. 반복되는 `REPIU_DOS_INT_TRACE` 전체 출력 대신
`REPIU_DOS_ATTR_TRACE=1`을 사용해 AH=43h 결과를 bounded하게 기록했습니다.

### 구현 내용

- `HandleDosFileAttributes`에 opt-in `REPIU_DOS_ATTR_TRACE`를 추가했습니다.
- 성공 8회, 실패 64회로 출력량을 제한했습니다.
- guest path, DOS virtual path, host path, DOS 오류 코드, EAX/ECX/EDX, CF를
  함께 기록했습니다.
- guest 문자열을 읽지 못하는 경우도 별도 bounded failure로 기록합니다.
- 파일 경로와 DOS/HLE 의미론은 변경하지 않았습니다.

### 검증

Linux x64 실제 실행 타깃과 core probe를 빌드했습니다.

```text
[100%] Built target repiu
[100%] Built target repiu_core_probe
```

전체 build는 별도 `repiu_instruction_census` 링크 단계에서 기존 OpenGL
심볼 부족으로 실패했지만, 이번 수정이 포함된 `repiu`와 core probe는
성공했습니다.

bounded 실행은 기존 fail-closed `SIGTRAP`까지 도달했습니다.

```text
[repiu-dos-attr] kind=failure n=1 eip=0x010F310B subfunction=0x00 guest_path=".ovl" dos_path="\\PIU\\.OVL" host_path="/mnt/e/MYWORK/Projects/rePIU/build/runtime_mounts/pumpit2a/PIU/.OVL" success=0 readable=1 error=0x0002 eax=0x00000002 ecx=0xFFFFFFFE edx=0x0138C679 cf=1
[repiu-dos-attr] kind=failure n=2 eip=0x010F310B subfunction=0x00 guest_path=".ovl" dos_path="\\PIU\\.OVL" host_path="/mnt/e/MYWORK/Projects/rePIU/build/runtime_mounts/pumpit2a/PIU/.OVL" success=0 readable=1 error=0x0002 eax=0x00000002 ecx=0x0158C828 edx=0x0158C828 cf=1
[repiu-dos-attr] kind=failure n=3 eip=0x010F310B subfunction=0x00 guest_path="C:\\WINDOWS\\SYSTEM\\.ovl" dos_path="\\WINDOWS\\SYSTEM\\.OVL" host_path="/mnt/e/MYWORK/Projects/rePIU/build/runtime_mounts/pumpit2a/WINDOWS/SYSTEM/.OVL" success=0 readable=1 error=0x0002 eax=0x00000002 ecx=0xFFFFFFEC edx=0x0138C679 cf=1
```

AH=43h 자체는 DOS 오류 `0x0002`와 CF=1을 반환합니다. 이후 F0B50 계열
wrapper가 실패를 zero 결과로 바꾸고, `0x010EFEDC`의 일반
`MOV [EBP+0x18],EAX`가 그 zero를 저장합니다. 따라서 `SIGTRAP`은 DOS 오류
결과가 공통 return thunk에서 합성된 결과가 아닙니다.

경로 buffer writer도 확인했습니다.

```text
[repiu-native-write-match] guest=0x010F0B9D bytes=88073C0074108A46 destination=0x0158C828 size=1 eax=0x0000002E ebx=0x0158C92C ecx=0xFFFFFFFE edx=0x011A6250 esi=0x01119BDC edi=0x0158C828 ebp=0x0158C848 esp=0x0158C824
[repiu-native-write-match] guest=0x010F0BC0 bytes=88073C0074108A46 destination=0x0138C679 size=1 eax=0x0000002E ebx=0x0158C92C ecx=0xFFFFFFFE edx=0x00000000 esi=0x0158C828 edi=0x0138C679 ebp=0x0158C848 esp=0x0158C824
```

원본 `.ovl` 상수의 첫 바이트가 local buffer와 전역 path buffer에 기록되어
AH=43h 입력은 이미 `.ovl`입니다. 정적 호출부에는 올바른
`PUSH 0x010EFE2C`가 있지만, F0B50 내부의 `PUSH [EBP+0x18]`가 일반
memory lowering으로 처리되고 있어 guest 인자 전달을 훼손할 가능성이
확인되었습니다. 다음 Task 669에서는 이 일반 `PUSH r/m32` lowering을
수정합니다.

### 결론

이번 작업에서 특정 EIP/ESP 예외처리는 추가하지 않았습니다. 문제의 다음
수정 지점은 DOS path lookup 자체가 아니라, x64 AOT에서 guest stack을
사용해야 하는 `PUSH r/m32`의 공통 lowering입니다.

## English

### Purpose

Confirm the common DOS/HLE result preceding Task 667's zero writer. Instead of
the unbounded-looking full `REPIU_DOS_INT_TRACE` output, the task uses the
bounded `REPIU_DOS_ATTR_TRACE=1` diagnostic for AH=43h.

### Implementation

- Added opt-in `REPIU_DOS_ATTR_TRACE` to `HandleDosFileAttributes`.
- Capped output at 8 successes and 64 failures.
- Recorded the guest path, DOS virtual path, host path, DOS error, EAX/ECX/EDX,
  and CF together.
- Reported guest-string read failures through a bounded failure record.
- Changed neither path rules nor DOS/HLE semantics.

### Verification

Built the Linux x64 runtime target and core probe.

```text
[100%] Built target repiu
[100%] Built target repiu_core_probe
```

The all-target build still fails at the separate `repiu_instruction_census`
link step because of pre-existing missing OpenGL symbols, but the modified
`repiu` and core probe targets succeeded.

The bounded run reached the existing fail-closed `SIGTRAP` and reported:

```text
[repiu-dos-attr] kind=failure n=1 eip=0x010F310B subfunction=0x00 guest_path=".ovl" dos_path="\\PIU\\.OVL" host_path="/mnt/e/MYWORK/Projects/rePIU/build/runtime_mounts/pumpit2a/PIU/.OVL" success=0 readable=1 error=0x0002 eax=0x00000002 ecx=0xFFFFFFFE edx=0x0138C679 cf=1
[repiu-dos-attr] kind=failure n=2 eip=0x010F310B subfunction=0x00 guest_path=".ovl" dos_path="\\PIU\\.OVL" host_path="/mnt/e/MYWORK/Projects/rePIU/build/runtime_mounts/pumpit2a/PIU/.OVL" success=0 readable=1 error=0x0002 eax=0x00000002 ecx=0x0158C828 edx=0x0158C828 cf=1
[repiu-dos-attr] kind=failure n=3 eip=0x010F310B subfunction=0x00 guest_path="C:\\WINDOWS\\SYSTEM\\.ovl" dos_path="\\WINDOWS\\SYSTEM\\.OVL" host_path="/mnt/e/MYWORK/Projects/rePIU/build/runtime_mounts/pumpit2a/WINDOWS/SYSTEM/.OVL" success=0 readable=1 error=0x0002 eax=0x00000002 ecx=0xFFFFFFEC edx=0x0138C679 cf=1
```

AH=43h itself returns DOS error `0x0002` with CF=1. The F0B50-family wrapper
then converts that failure to zero, and the ordinary
`MOV [EBP+0x18],EAX` at `0x010EFEDC` stores the zero. The `SIGTRAP` is therefore
not a synthesized result from the common return thunk.

Path-buffer provenance was also observed:

```text
[repiu-native-write-match] guest=0x010F0B9D bytes=88073C0074108A46 destination=0x0158C828 size=1 eax=0x0000002E ebx=0x0158C92C ecx=0xFFFFFFFE edx=0x011A6250 esi=0x01119BDC edi=0x0158C828 ebp=0x0158C848 esp=0x0158C824
[repiu-native-write-match] guest=0x010F0BC0 bytes=88073C0074108A46 destination=0x0138C679 size=1 eax=0x0000002E ebx=0x0158C92C ecx=0xFFFFFFFE edx=0x00000000 esi=0x0158C828 edi=0x0138C679 ebp=0x0158C848 esp=0x0158C824
```

The first byte of the original `.ovl` constant is written into the local and
global path buffers, so AH=43h already receives `.ovl`. The static caller has
the expected `PUSH 0x010EFE2C`, but F0B50 contains `PUSH [EBP+0x18]`, which is
currently treated as ordinary memory lowering and can corrupt guest-argument
passing. Task 669 will fix this common `PUSH r/m32` lowering.

### Conclusion

No EIP/ESP-specific exception was added. The next fix point is the common x64
AOT lowering for `PUSH r/m32`, which must use the guest stack rather than the
host stack.
