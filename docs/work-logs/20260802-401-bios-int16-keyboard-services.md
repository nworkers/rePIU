# 20260802-401 BIOS INT 16h 구현 작업 로그 / BIOS INT 16h Implementation Work Log

## 한국어

### 작업 요약

`INT 16h`(BIOS 키보드)를 구현하고, 미지원 인터럽트 벡터가 로그에 이름을 남기지 않던
진단 공백을 닫았습니다. 그 결과 **pumpit3가 부팅해서 렌더 루프를 돌기 시작했습니다.**

사용자 지시에 따라 이번에는 직접 실행해 검증했습니다.

### Task 399가 폴링 정지를 해소했음을 분리 확인

| 실행 | hotspot profile | 결과 |
|---|---|---|
| 사용자 13:37 (Task 399 이전) | off | 폴링 루프에서 60초 이상 정지 |
| 15:07 census (Task 399 이후) | on | 폴링 통과, `0x03011537` 종료 |
| 15:09 대조 (Task 399 이후) | **off** | 폴링 통과, `0x03011537` 종료 |

프로파일러를 끈 대조 실행이 같은 지점에 도달했으므로 정지 해소의 원인은 계측
오버헤드가 아니라 Task 399 수정입니다. 두 실행 모두
`INT 8 chain HLE ... target: 0x0000002B:0x00000000`을 기록했습니다(이전 `0x03010000`).

### 원인 확정 근거 (INT 16h)

`0x03011537`, `0xC0000005`. byte window가 지시한 루틴은 `AH=12h`(확장 shift flags) →
`AH=11h`(확장 keystroke 확인) → 키가 있을 때만 `AH=10h`(읽기) 순으로 호출하는 키보드
조회 루틴입니다. `mov dh, ah`로 함수 번호를 보존하고 ZF에 따라 `dh` 하위 니블을 검사하는
fixup은 확장 함수 미지원 BIOS 대비 코드입니다. 바이너리의 `CD 16` 지점은 8곳입니다.

### 변경 내용

1. `src/platform/win32/bios/bios_keyboard_services.{h,cpp}` 신규:
   `HandleBiosInterrupt16`. `AH=00/01/10/11`은 `AX=0` + ZF set(버퍼 비어 있음),
   `AH=02/12`는 `AX=0` + ZF clear(shift 없음), 그 외는 `hle_message` 기록 후 미처리.
   캐비닛 입력은 `0x02A0` 계열 포트로 들어오므로 게스트 키보드가 실제 유휴 상태이고,
   "키 없음"은 stub이 아니라 정확한 상태 보고입니다.
2. `CMakeLists.txt`: 새 소스와 `src/platform/win32/bios` include 경로 등록.
3. `execution_trampoline.cpp`: `HandleDosHleInstruction`에 `CD 16` 분기.
4. `instruction_emulation.{h,cpp}`: `HandleTracedBiosInterrupt16` 추가 후 traced
   dispatch chain 3곳에 연결.
5. `RecordUnsupportedTracedSoftwareInterrupt` 추가. VEH에서 소프트웨어 인터럽트를
   처리할 수 있는 handler를 모두 지난 뒤 `unsupported software interrupt 0xNN`을
   기록합니다. 이 지점 이후 handler는 `CD` 명령을 처리하지 않으므로 오탐이 없고,
   `hle_message`가 이미 있으면 덮어쓰지 않습니다.
6. Task 400 dump 견고화: 45초 interrupted 실행에서 teardown이 `glide_backend.Close()`
   이후 5분 넘게 멈추는 것을 관측했고, dump가 그 뒤에 있어 census 전체를 잃었습니다.
   dump를 게스트 스레드 정지 직후로 옮기고 write-once 플래그를 두어 이후 보고 경로가
   같은 값을 다시 읽되 파일은 다시 쓰지 않게 했습니다.

### 검증 결과 (직접 실행, 45초 timeout)

- 빌드: `cmake --build build --config Release --target repiu_loader_win32` 성공.
- `0x03011537` 종료 소멸. 종료 사유는 `minimal execution attempt timed out`입니다.
- **`_GRBUFFERSWAP@4` count=1,140** — 45초에 약 1,140 프레임(약 25 FPS)을 그렸습니다.
  이전까지는 단 한 번도 호출되지 않았습니다.
- `Win32 Glide window opens/logical size: 1/640x480`,
  texture uploads/distinct `27/24`.
- `Win32 INT 8 chain HLE count: 696`, MSCDEX `available/audio/tracks: true/true/65`.
- `Win32 DOS AH hotspots [2C:273122 11:1139 12:1139 4A:110]` — `INT 16h` `AH=11h/12h`가
  프레임당 1회씩 처리됩니다.
- census: `total/distinct/overflow = 287,599/122/0`.

### census가 지목한 다음 비용 (미확정, 다음 과제 후보)

census 상위 3개가 전체 표본의 약 95%입니다.

| 주소 | 표본 | 정체 |
|---|---:|---|
| `0x030D395B` | 116,805 | `INT 21h AH=2Ch` 지연 루프 |
| `0x030D394B` | 97,912 | 같은 루틴의 초 변화 대기 |
| `0x030D3997` | 58,403 | 같은 루틴의 지연 본체 |

즉 게임의 시간 지연 루틴이 `INT 21h AH=2Ch`를 초당 약 6,000회 호출하고 매 호출이
예외 왕복입니다. 원본 DOS에서는 훨씬 싼 호출이므로, 현재 약 25 FPS의 주된 비용일
가능성이 높습니다. **아직 측정으로 확정하지는 않았습니다.**

---

## English

### Summary

Implemented `INT 16h` (BIOS keyboard) and closed the diagnostic gap where an unsupported
interrupt vector was never named in the log. As a result **pumpit3 now boots and runs its
render loop.** Per the user's instruction, verification was done by running it directly.

### Isolating Task 399 as what cleared the polling stall

| Run | hotspot profile | Result |
|---|---|---|
| User 13:37 (pre-399) | off | Stalled in the polling loop for 60+ seconds |
| 15:07 census (post-399) | on | Passed the poll, terminated at `0x03011537` |
| 15:09 control (post-399) | **off** | Passed the poll, terminated at `0x03011537` |

The control run with profiling disabled reached the same point, so instrumentation overhead
did not clear the stall — the Task 399 fix did. Both logged
`INT 8 chain HLE ... target: 0x0000002B:0x00000000` (previously `0x03010000`).

### Evidence for the INT 16h root cause

At `0x03011537` with `0xC0000005`. The routine the byte window points at queries the
keyboard: `AH=12h` (extended shift flags), `AH=11h` (check for an extended keystroke), and
`AH=10h` (read it) only when the check reported one. It preserves the function number with
`mov dh, ah` and inspects the low nibble of `dh` when ZF returns set — a fixup for BIOSes
without the extended calls. The binary has eight `CD 16` sites.

### Changes

1. New `src/platform/win32/bios/bios_keyboard_services.{h,cpp}` with
   `HandleBiosInterrupt16`: `AH=00/01/10/11` return `AX=0` with ZF set (empty buffer),
   `AH=02/12` return `AX=0` with ZF clear (no shift keys), anything else records
   `hle_message` and stays unhandled. Cabinet inputs arrive over the `0x02A0` port family,
   so the guest keyboard is genuinely idle and "no key" is an accurate report, not a stub.
2. `CMakeLists.txt`: registered the source and the `src/platform/win32/bios` include path.
3. `execution_trampoline.cpp`: added the `CD 16` branch to `HandleDosHleInstruction`.
4. `instruction_emulation.{h,cpp}`: added `HandleTracedBiosInterrupt16` and wired it into
   the three traced dispatch chains.
5. Added `RecordUnsupportedTracedSoftwareInterrupt`, called once every VEH handler that can
   service a software interrupt has declined, recording
   `unsupported software interrupt 0xNN`. No handler past that point services a `CD`
   instruction, so there are no false positives, and it never overwrites an existing
   `hle_message`.
6. Hardened the Task 400 dump: a 45-second interrupted run was observed hanging in teardown
   after `glide_backend.Close()` for over five minutes, and the dump sat behind it, losing
   the whole census. The dump now runs immediately after the guest thread stops, with a
   write-once flag so the later reporting path reads the same numbers without rewriting the
   file.

### Verification results (direct run, 45-second timeout)

- Build: `cmake --build build --config Release --target repiu_loader_win32` succeeded.
- The `0x03011537` termination is gone; the run ends with
  `minimal execution attempt timed out`.
- **`_GRBUFFERSWAP@4` count = 1,140** — about 1,140 frames in 45 seconds (~25 FPS). It had
  never been called before.
- `Win32 Glide window opens/logical size: 1/640x480`, texture uploads/distinct `27/24`.
- `Win32 INT 8 chain HLE count: 696`; MSCDEX `available/audio/tracks: true/true/65`.
- `Win32 DOS AH hotspots [2C:273122 11:1139 12:1139 4A:110]` — `INT 16h` `AH=11h/12h` are
  serviced once per frame.
- Census: `total/distinct/overflow = 287,599/122/0`.

### What the census points at next (unresolved, candidate task)

The top three census entries are about 95% of all samples:

| Address | Samples | What it is |
|---|---:|---|
| `0x030D395B` | 116,805 | `INT 21h AH=2Ch` delay loop |
| `0x030D394B` | 97,912 | The seconds-change wait in the same routine |
| `0x030D3997` | 58,403 | The delay body in the same routine |

The game's timing-delay routine calls `INT 21h AH=2Ch` roughly 6,000 times per second, and
every call is an exception round trip. That call is far cheaper on original DOS, so it is
likely the dominant cost behind the current ~25 FPS. **This has not been confirmed by
measurement yet.**
