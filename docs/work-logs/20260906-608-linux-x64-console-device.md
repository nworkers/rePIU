# 작업 로그 20260906-608 — Linux x64 DOS `CON` device HLE

## 결과

Linux x64 `pumpit2a`에서 `INT 21h AH=3Dh`가 요청한 `con`을 host regular
file로 해석하던 문제를 DOS `CON` character device HLE로 분리했습니다.
`con`은 DOS user handle `0x0005`로 열리고 host `CON` 파일은 생성되지
않습니다. `AH=40h` 쓰기는 console output sink로 전달됩니다.

이번 작업은 원본 guest 코드나 특정 guest EIP를 수정하지 않았습니다.

## 구현

1. `DosOpenFileHandle`에 character-device 상태를 추가했습니다.
2. `CON`과 `CON:`을 DOS 경로의 마지막 구성요소에서 대소문자 구분 없이
   인식하고, regular-file existence 검사 전에 user handle을 할당하도록
   했습니다.
3. `AH=40h`에서 `CON` handle을 `AppendConsoleOutput`으로 라우팅하고,
   요청 바이트 수를 성공 결과로 반환하도록 했습니다.
4. open/no-host-file core probe를 추가했습니다.
5. `REPIU_DOS_INT_TRACE`와 `REPIU_LINEXE_INIT_TRACE` opt-in 진단을 추가해
   DOS 인터럽트의 guest register와 LINEXE 초기화 단계별 결과를 기록했습니다.

## 검증

빌드:

```text
wsl.exe -d Ubuntu-24.04 -- bash -lc "cd /mnt/e/MYWORK/Projects/rePIU && cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2"
```

core probe는 `24/24` 성공이었고, i386 assembly probe 두 항목은 기존처럼
x64 host에서 skip되었습니다.

direct dispatch 기본 실행의 초기화 trace:

```text
[repiu-linexe-init] extracted=1 plan=1 layout=1 glide_fits=0 direct=1 writes=1/1/0/0/0 verify=0 descriptors=0 protect=0/0/0/0/0 active=0
```

이 상태에서는 `AX=FF00h` 진입 시 `linexe=0`, `GS=0`이고 `EAX=FF00h`가
그대로 반환되었습니다. 원인은 Linux x64에서 32비트 guest image에 넣을
direct-dispatch thunk가 없기 때문입니다.

direct dispatch를 명시적으로 끈 A/B 실행에서는 다음이 확인되었습니다.

```text
[repiu-linexe-init] extracted=1 plan=1 layout=1 glide_fits=1 direct=0 writes=1/1/1/1/1 verify=1 descriptors=1 protect=1/1/1/1/1 active=1
[repiu-dos-int-context] phase=return eip=0x010F17EE eax=0x0000FFFF ... gs=0x0020 linexe=1
[repiu-dos-int-context] phase=return eip=0x010F194E eax=0x01110005 ... gs=0x0020 linexe=1
```

`CON` open과 write는 성공했지만 guest는 여전히
`Not enough memory to allocate file structures`를 출력한 뒤 `AX=4C01h`로
종료했습니다. 따라서 `CON` 문제는 해결되었고, 남은 문제는 별도의
LINEXE/DPMI 또는 guest initialization frontier입니다. 실행 중
`[repiu-fault]`, `Segmentation fault`, `Illegal instruction`, `core dumped`는
발생하지 않았습니다.

## 다음 작업

Linux x64에서 direct-dispatch capability를 확인하지 않고 실패한 결과로
전체 LINEXE 환경을 비활성화하는 정책을 수정해야 합니다. direct patch가
불가능하면 기존 trap/HLE Glide gate image를 유지하여 LINEXE 초기화를
완료시키는 후속 작업으로 분리합니다.

## English

### Result

The Linux x64 `pumpit2a` path now treats the observed `con` requested by
`INT 21h AH=3Dh` as the DOS `CON` character device instead of a host regular
file. It opens as DOS user handle `0x0005` without creating a host `CON` file,
and `AH=40h` routes writes to the console output sink.

No original guest bytes or guest-EIP exception were added.

### Implementation

1. Added character-device state to `DosOpenFileHandle`.
2. Recognized `CON` and `CON:` case-insensitively in the final DOS path
   component before regular-file existence checks, allocating a normal user
   handle.
3. Routed `CON` handles from `AH=40h` to `AppendConsoleOutput` and returned
   the requested byte count on success.
4. Added an open/no-host-file core probe.
5. Added opt-in `REPIU_DOS_INT_TRACE` and `REPIU_LINEXE_INIT_TRACE` diagnostics
   for guest registers and LINEXE initialization stages.

### Verification

The Linux x64 build completed and the core probe passed `24/24`; the two i386
assembly probes remained skipped on the x64 host as before.

With direct dispatch at its default setting, the initialization trace was:

```text
[repiu-linexe-init] extracted=1 plan=1 layout=1 glide_fits=0 direct=1 writes=1/1/0/0/0 verify=0 descriptors=0 protect=0/0/0/0/0 active=0
```

The `AX=FF00h` entry consequently saw `linexe=0`, `GS=0`, and returned
`EAX=FF00h` unchanged. The cause is that Linux x64 has no thunk address that
can be embedded in the 32-bit guest direct-dispatch image.

With `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH=0`, the existing trap/HLE gate image
completed initialization:

```text
[repiu-linexe-init] extracted=1 plan=1 layout=1 glide_fits=1 direct=0 writes=1/1/1/1/1 verify=1 descriptors=1 protect=1/1/1/1/1 active=1
[repiu-dos-int-context] phase=return eip=0x010F17EE eax=0x0000FFFF ... gs=0x0020 linexe=1
[repiu-dos-int-context] phase=return eip=0x010F194E eax=0x01110005 ... gs=0x0020 linexe=1
```

`CON` open and write succeeded, but the guest still printed
`Not enough memory to allocate file structures` and terminated with
`AX=4C01h`. The remaining issue is therefore a separate LINEXE/DPMI or guest
initialization frontier. No `[repiu-fault]`, segmentation fault, illegal
instruction, or core dump occurred.

### Next task

Make the Linux x64 direct-dispatch policy capability-aware. If the optional
direct patch cannot be installed, retain the existing trap/HLE Glide gate image
so LINEXE initialization can complete. Track the subsequent DPMI frontier in a
separate work order.
