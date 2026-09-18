# Task 706 설계 — Linux x64 telemetry source의 플랫폼 편성

## 배경과 확인된 원인

Win32 x86 Debug 전체 빌드는 `linux_x64_native_write_trace.cpp`를 컴파일하면서
`linux_x64_aot_frame.h`의 `sizeof(LinuxX64HostPointer) == 8` 검증에서 실패했습니다.
`main`의 CMake에도 이 소스가 공용 `repiu_exe` 목록에 포함되어 있어 동일한 구성
결함이 존재합니다.

소스 내부 구현은 `_WIN32`와 `__x86_64__`로 보호되지만, Linux x64 ABI header include가
그 조건문보다 앞에 있습니다. 더 근본적으로 이 translation unit은 Linux SysV x64
dispatch frame만 해석하므로 Win32 x86 target에 편성할 이유가 없습니다.

## 설계

`linux_x64_native_write_trace.cpp`를 공용 source 목록에서 제거하고, 기존 Linux x64
assembly/dispatch source와 같은 `UNIX AND NOT EMSCRIPTEN AND pointer-size > 4` 조건에
편성합니다. 소스 내부 guard는 방어 계층으로 유지합니다.

Linux x64 ABI 구조체의 pointer width assertion은 약화하지 않습니다. Win32를 통과시키기
위해 ABI 검증을 끄는 대신, 해당 ABI를 사용하지 않는 target이 파일을 컴파일하지 않게
합니다.

## 검증 전략

- Win32 x86 Debug 전체 빌드를 끝까지 완료합니다.
- Linux x64 Debug `repiu`와 `repiu_core_probe`를 빌드합니다.
- Linux x64 core probe 27/27을 실행합니다.
- CMake source 편성이 Linux x64에서 파일을 유지하고 Win32 x86에서 제외하는지 확인합니다.

---

## English

### Background and confirmed cause

The complete Win32 x86 Debug build failed while compiling
`linux_x64_native_write_trace.cpp`, at the
`sizeof(LinuxX64HostPointer) == 8` assertion in `linux_x64_aot_frame.h`. The
same configuration defect exists on `main`: the source is in the common
`repiu_exe` source list.

The implementation body is guarded by `_WIN32` and `__x86_64__`, but the Linux
x64 ABI header is included before that guard. More importantly, this translation
unit decodes only the Linux SysV x64 dispatch frame and has no role in a Win32
x86 target.

### Design

Remove `linux_x64_native_write_trace.cpp` from the common source list and add it
beside the existing Linux x64 assembly/dispatch sources under the
`UNIX AND NOT EMSCRIPTEN AND pointer-size > 4` condition. Keep its internal
guard as defense in depth.

Do not weaken the Linux x64 ABI pointer-width assertion merely to make Win32
pass. Targets that do not use that ABI must not compile the translation unit.

### Verification strategy

Complete the full Win32 x86 Debug build; build Linux x64 Debug `repiu` and
`repiu_core_probe`; run all 27 Linux x64 core-probe groups; and confirm that
CMake retains the source on Linux x64 while excluding it from Win32 x86.
