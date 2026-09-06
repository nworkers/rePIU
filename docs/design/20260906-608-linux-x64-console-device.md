# Linux x64 DOS `CON` device HLE 설계

## 목적

Task 607 이후 `pumpit2a`는 Linux x64에서 fault 없이 실행되지만,
`INT 21h AH=3Dh`로 요청한 `con`을 일반 파일로 해석하여 DOS error `0x0002`를
반환합니다. 원본 게스트는 이어서 오류 메시지를 출력하고 `AX=4C01h`로
종료하므로, 다음 실행 frontier는 CPU가 아니라 DOS character device 경계입니다.

## 확인된 근거

* guest path는 소문자 `con`, access mode는 `AL=01h`(write-only)입니다.
* 현재 path trace는 `virtual=\PIU\CON`, host path는
  `.../PIU/CON`, result는 failure/error `0x0002`입니다.
* 실제 mount에는 `CON` regular file이 없고, 이를 파일로 생성하는 것은 DOS
  device 의미를 보존하지 않습니다.
* 같은 실행에서 Glide, port I/O, LINEXE module load는 모두 0이며, guest는
  초기화 이후로 진행하지 못합니다.

## 검증 후 판정

`CON` device open을 구현한 뒤 실제 Linux x64 실행에서 `con`은 DOS handle
`0x0005`로 성공했고 host `CON` 파일은 만들어지지 않았습니다. 그러나 guest는
여전히 같은 `Not enough memory to allocate file structures` 문자열을
출력하고 `AX=4C01h`로 종료했습니다. 따라서 이 작업의 다음 진단은 `CON` open
자체가 아니라 `AX=FF00h` 직후의 guest 초기화 분기와 그 반환 레지스터입니다.
`REPIU_DOS_INT_TRACE=1`에서 각 `INT 21h`의 guest EIP와 호출·반환 레지스터를
출력하여 이 분기를 원본 코드 주소로 귀속합니다.

## 설계 결정

1. VFS path resolver의 일반 파일 규칙과 DOS device 판정을 분리합니다. 현재
   관찰된 canonical device 이름 `CON`(대소문자 무관, 선택적인 마지막 `:`)만
   device로 인식하며 host 파일을 만들거나 요구하지 않습니다.
2. `CON`도 DOS handle table의 일반 사용자 handle 번호를 받아 open/close
   수명과 handle 재사용 규칙을 유지합니다. device handle에는 host path가
   없고, writable 상태는 open access mode에서 결정합니다.
3. `AH=40h`에서 device handle은 VFS file write로 보내지 않고 기존
   `AppendConsoleOutput` 경계로 보냅니다. 반환값은 요청 byte 수, CF clear로
   하여 DOS character device의 write 계약을 유지합니다.
4. 이번 범위는 실제 관찰된 write-only `CON` open/write입니다. `CON` read,
   keyboard input, IOCTL 세부 의미는 추측하여 추가하지 않고 별도 frontier로
   남깁니다.

## 비대상

* `CON` 이름의 host regular file 생성
* 특정 guest EIP를 우회하는 예외 처리
* 전체 DOS device namespace 또는 keyboard input의 추정 구현
* 원본 게임 로직의 C++ 재구현

## 검증 전략

* VFS probe에서 `con` open이 성공하고 사용자 handle을 받으며 host file이
  생성되지 않는지 확인합니다.
* traced `AH=40h` probe에서 device handle의 bytes가 console output으로
  기록되고 file write counter는 증가하지 않는지 확인합니다.
* 기존 regular-file create/read/write/close probe와 Linux x64 core probe를
  회귀 실행합니다.
* `pumpit2a`에서 `DOS path trace`가 `con` success/handle을 기록하고,
  기존 초기화 오류 메시지와 즉시 `AX=4C01h` 종료가 사라지는지 확인합니다.
  오류가 유지되면 `CON` 문제와 별개의 guest 초기화 frontier로 기록합니다.

## English

### Purpose

After Task 607, `pumpit2a` runs without a Linux x64 fault, but
`INT 21h AH=3Dh` opens `con` as though it were a regular file and returns DOS
error `0x0002`. The original guest then prints an initialization error and exits
with `AX=4C01h`, so the next execution frontier is the DOS character-device
boundary rather than the CPU path.

### Confirmed evidence

* The guest path is lowercase `con` and the access mode is `AL=01h` (write-only).
* The path trace reports `virtual=\PIU\CON`, host path `.../PIU/CON`, and
  failure/error `0x0002`.
* The mount has no `CON` regular file, and creating one would not preserve DOS
  device semantics.
* Glide, port I/O, and LINEXE module loads are all zero in the same run; the
  guest does not advance beyond initialization.

### Design decisions

1. Separate DOS-device recognition from the VFS regular-file rule. Recognize the
   observed canonical device name `CON` case-insensitively, with an optional
   final colon, without creating or requiring a host file.
2. Give `CON` a normal DOS user handle so open/close lifetime and handle reuse
   remain covered by the existing table. A device handle has no host path, and
   its writable state comes from the open access mode.
3. Route a device handle at `AH=40h` to the existing `AppendConsoleOutput`
   boundary instead of VFS file write. Return the requested byte count with CF
   clear, matching the observed DOS character-device write contract.
4. Limit this task to the observed write-only `CON` open/write. Do not guess
   `CON` read, keyboard input, or detailed IOCTL semantics; record those as a
   separate frontier.

### Out of scope

* Creating a host regular file named `CON`
* An exception bypass for a specific guest EIP
* Guessing the full DOS device namespace or keyboard input
* Reimplementing game logic in C++

### Verification strategy

* A VFS probe verifies that opening `con` succeeds with a user handle and does
  not create a host file.
* A traced `AH=40h` probe verifies that device bytes reach console output and
  do not increment the file-write counter.
* Run the existing regular-file create/read/write/close probe and Linux x64
  core probe as regression checks.
* In `pumpit2a`, verify that the DOS path trace reports a successful `con` open
  and handle. If the initialization error followed by immediate `AX=4C01h`
termination remains, record it as a separate guest-initialization frontier
rather than attributing it to `CON`.

## 추가 진단 결과

초기화 trace는 첫 실패 단계를 선택적 Glide direct-dispatch patch로
좁혔습니다. 추출된 LINEXE segment, call-gate plan, arena layout은 모두
유효했습니다. Linux x64에서 direct dispatch를 켜면 32비트 guest image에
넣을 host thunk가 없어 patch가 실패하고, 그 결과 이후 LINEXE image 기록과
descriptor 등록까지 진행되지 않습니다.

`REPIU_AOT_DBT_GLIDE_GATE_DISPATCH=0`에서는 기존 trap/HLE gate image가
초기화를 완료하고 DOS/4GW 식별 호출이 `EAX=0000FFFFh`, `GS=0020h`로
반환되었습니다. 그래도 guest는 `CON`을 열고 쓴 뒤 종료하므로 남은
초기화 오류는 별도의 DPMI 또는 guest-runtime frontier입니다. capability
기반 자동 fallback은 다음 작업으로 분리합니다.

## English

### Additional diagnostic result

The initial trace identified the first failing stage as the optional Glide
direct-dispatch patch. All extracted LINEXE segments, the call-gate plan, and
the arena layout were valid. With direct dispatch enabled on Linux x64, the
host-side thunk is unavailable to the 32-bit guest image, so the patch failed
and prevented all later LINEXE writes and descriptor registration.

With `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH=0`, the existing trap/HLE gate image
completed initialization and the DOS/4GW identification call returned
`EAX=0000FFFFh` with `GS=0020h`. The guest still terminated after opening and
writing `CON`, so the remaining initialization error is a separate DPMI or
guest-runtime frontier. Automatic capability-based fallback is tracked as the
next task.
