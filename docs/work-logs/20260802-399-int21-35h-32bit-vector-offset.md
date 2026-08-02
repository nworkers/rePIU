# 20260802-399 INT 21h AH=35h 32비트 offset 수정 작업 로그 / INT 21h AH=35h 32-bit Offset Fix Work Log

## 한국어

### 작업 요약

Task 398이 별도 Task로 남긴 `HandleDosGetInterruptVector`의 16비트 절단 결함을
수정했습니다. 사용자 요청에 따른 작업입니다.

### Task 398 수정의 실행 결과 (사용자 제공 로그, 13:37)

- `Win32 INT 8 chain HLE count/source/pointer/target:`
  `5/0x0301F827/0x0343ED08/0x0000002B:0x03010000`
- 체인 인식이 동작했고 `0x0301F827` 크래시는 소멸했습니다.
- 66초까지 종료 없이 실행됐고 사용자가 직접 종료했습니다
  (`minimal execution stopped by SDL exit request`).

로그의 target offset `0x03010000`이 본 Task가 고칠 결함을 실측으로 확인해 줍니다.

### 변경 내용

`src/platform/win32/dos/dos_int21_services.cpp`의 `HandleDosGetInterruptVector`:

1. 값 출처를 `dpmi_interrupt_vectors[vector]` 우선으로 바꾸고, 없으면
   `dos_interrupt_vectors[vector]`로 fallback합니다. DOS extender는 32비트 클라이언트의
   `AH=35h`를 protected mode 벡터 조회로 처리하며, 이는 `AH=25h`가 기록하고 `AX=0204`가
   읽는 표입니다.
2. `Ebx = (Ebx & 0xFFFF0000U) | offset`을 `Ebx = offset`으로 바꿔 32비트 offset 전체를
   기록합니다. 미설치 벡터는 `0`입니다.

`ES`/`guest_es` 처리와 carry clear는 변경하지 않았습니다.

### 검증 결과

- 빌드: `cmake --build build --config Release --target repiu_loader_win32` 성공,
  신규 경고 없음.
- **실행 검증 대기 중.** pumpit3 로그에서 chain target offset이 `0x03010000` →
  `0x00000000`으로 바뀌고 count가 계속 증가하는지, pumpit1/pumpit2에서
  `INT 8 chain HLE count` 회귀가 없는지 확인해야 합니다.

### 함께 기록한 새 frontier: 진행 정지

크래시는 사라졌지만 게임이 진행하지 않습니다. 확인된 사실만 적으면 다음과 같습니다.

- 약 5초부터 66초까지 `last_eip`가 `0x0301DB1F`~`0x0301DB2A`에 고정되고 `progress`는
  `7591`에서 움직이지 않았습니다.
- 그 구간은 입력 폴링 루틴 `0x0301DB10`입니다. 포트 `0x02A8`을 200회 읽어 값을 버리는
  I/O 지연 루프 뒤, `[0x030F9028] mod 4`로 4개 상태(`0x0301DB4D`, `0x0301DF8E`,
  `0x0301E3D3`, `0x0301E816`)에 분기하며 각 상태가 `0x02A4`/`0x02A6`에 strobe를 씁니다.
  발판 센서 뱅크 멀티플렉싱입니다.
- `0x0301DB10`의 유일한 호출자는 `0x03010BCF`(주기 서비스 루틴)입니다.
- Glide 게이트는 `#51 _GRTEXDOWNLOADMIPMAPLEVEL@32`에서 멈췄고 `_GRBUFFERSWAP`은 한 번도
  호출되지 않았습니다.

**미확정:** 무엇을 기다리는지는 확정하지 못했습니다. `last_eip` 단일 샘플로는 폴링이
정상 주기 호출인지 바깥 루프가 갇힌 것인지 구분할 수 없습니다. 다음 단계로 이 구간의
EIP 히스토그램 또는 `0x03010BCF` 호출자 체인 캡처를 제안합니다.

---

## English

### Summary

Fixed the 16-bit truncation defect in `HandleDosGetInterruptVector` that Task 398 deferred
to its own task, at the user's request.

### What the Task 398 fix produced at runtime (user log, 13:37)

- `Win32 INT 8 chain HLE count/source/pointer/target:`
  `5/0x0301F827/0x0343ED08/0x0000002B:0x03010000`
- Chain recognition worked and the `0x0301F827` crash is gone.
- The run continued for 66 seconds without terminating and was stopped by the user
  (`minimal execution stopped by SDL exit request`).

The `0x03010000` target offset in that line is the measured evidence for the defect fixed
here.

### Changes

In `HandleDosGetInterruptVector` (`src/platform/win32/dos/dos_int21_services.cpp`):

1. Read `dpmi_interrupt_vectors[vector]` first, falling back to
   `dos_interrupt_vectors[vector]`. A DOS extender serves `AH=35h` from a 32-bit client out
   of the protected-mode vector, the table `AH=25h` writes and `AX=0204` reads.
2. Replace `Ebx = (Ebx & 0xFFFF0000U) | offset` with `Ebx = offset` so the full 32-bit
   offset is returned; an uninstalled vector yields `0`.

`ES`/`guest_es` handling and the carry clear are unchanged.

### Verification results

- Build: `cmake --build build --config Release --target repiu_loader_win32` succeeded with
  no new warnings.
- **Runtime verification pending.** A pumpit3 log should show the chain target offset change
  from `0x03010000` to `0x00000000` with the count still rising, and pumpit1/pumpit2 should
  show no regression in `INT 8 chain HLE count`.

### New frontier recorded alongside: the stall

The crash is gone but the game does not advance. Confirmed facts only:

- From about 5 s to 66 s, `last_eip` stayed within `0x0301DB1F`-`0x0301DB2A` and `progress`
  never moved from `7591`.
- That range is the input polling routine at `0x0301DB10`: an I/O delay reading port
  `0x02A8` 200 times and discarding each value, then a dispatch on `[0x030F9028] mod 4` to
  four states (`0x0301DB4D`, `0x0301DF8E`, `0x0301E3D3`, `0x0301E816`), each strobing ports
  `0x02A4`/`0x02A6` — sensor-bank multiplexing.
- The only caller of `0x0301DB10` is `0x03010BCF`, a periodic service routine.
- Glide gates stopped at `#51 _GRTEXDOWNLOADMIPMAPLEVEL@32` and `_GRBUFFERSWAP` was never
  called.

**Unresolved:** what the game is waiting for. A single `last_eip` sample cannot separate
normal periodic polling from an outer loop stuck on a condition. The proposed next step is
an EIP histogram over this window or a caller-chain capture at `0x03010BCF`.
