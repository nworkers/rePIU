# 20260802-397 DOS INT 21h AH=2Ch 구현 작업 로그 / DOS INT 21h AH=2Ch Implementation Work Log

## 한국어

### 작업 요약

`pumpit3`가 초기화 후반부에서 멈추던 원인을 사용자 제공 로그와 원본 실행 파일 대조로
확정하고, 미구현 DOS 서비스 `INT 21h AH=2Ch`(Get System Time)를 구현했습니다.

### 원인 확정 근거

1. 로그의 32바이트 window
   `00 00 00 5E 5A 5B C3 31 C0 C3 53 51 52 56 B4 2C [CD] 21 ...`가
   `build/runtime_mounts/pumpit3/PIU/PIU.EXE` offset `0xDEB31`과 바이트 단위로
   일치했습니다. faulting EIP `0x030D3941` = offset `0xDEB41`의 `int 21h`입니다.
2. `0xDEB3B`의 루틴은 `AH=2Ch`로 `DH`(초)가 바뀔 때까지 대기한 뒤 1초 동안 호출 횟수를
   세어 `0x0041CD2C`에 저장하는 delay-loop 보정 루틴입니다.
3. `HandleDosInterrupt21`의 `switch (ah)`에 `0x2C`가 없어 `default` 분기가
   `unsupported DOS INT 21h AH=0x2c`로 실패했고, 이것이 로그의
   `Current execution blocker: unhandled HLE trap candidate`입니다.
4. `B4 2C CD 21` 패턴 계수: pumpit1 1곳(`0x10BD83`), pumpit2 1곳(`0x107A95`) —
   둘 다 호출되지 않는 Watcom `_dos_gettime` 영역. pumpit3는 같은 라이브러리 지점
   `0xDDED9` 외에 게임 코드 4곳(`0xDEB41` `0xDEB4B` `0xDEB5B` `0xDEB97`)에서 호출합니다.
   pumpit1/pumpit2에서 드러나지 않은 이유가 이것입니다.

### 변경 내용

1. `src/platform/win32/dos/dos_int21_services.h`:
   - `HandleDosGetSystemTime` 선언 추가.
2. `src/platform/win32/dos/dos_int21_services.cpp`:
   - `HandleDosGetSystemTime` 구현. `GetLocalTime`으로 호스트 local time을 읽어
     `CH:CL` = 시:분, `DH:DL` = 초:1/100초를 반환하고 carry를 clear합니다.
     `ECX`/`EDX` 상위 16비트와 `EAX`는 보존합니다.
   - `HandleDosInterrupt21`의 `switch (ah)`에 `case 0x2C` 추가.
3. `src/platform/win32/cpu_emul/instruction_emulation.cpp`:
   - `HandleTracedDosInterrupt21`의 `switch (ah)`에 `case 0x2C` 추가.
     dispatch 표가 실행 backend별로 갈리므로 한쪽만 고치면 같은 중단이 재현됩니다.
4. 문서:
   - `docs/design/20260802-397-dos-int21-2c-system-time.md` 신규
   - `docs/work-orders/20260802-397-dos-int21-2c-system-time.md` 신규
   - `docs/analysis/interrupts-and-port-io.md` Task 397 항목 추가, 지원 서비스 목록 갱신
   - `docs/analysis/current-execution-frontier.md` Task 397 보조 frontier 추가
   - `docs/kb/important-interrupts.md` AH=2Ch 레지스터 규약 추가

### 검증 결과

- 빌드: `cmake --build build --config Release --target repiu_loader_win32` 성공.
  신규 경고 없음(기존 C4819 코드페이지 경고만 출력).
- **실행 검증 미완료.** 프로젝트 규칙에 따라 게임 반복 실행 대신 사용자 제공 로그로
  판정하므로, `repiu_host --target pumpit3` 로그에서
  `Current execution blocker: unhandled HLE trap candidate` 부재와 `0x030D3941` 통과를
  확인하는 단계가 남아 있습니다. `pumpit1`/`pumpit2` 회귀 확인도 같은 단계입니다.

### 미확정으로 남긴 것

보정 계수는 한 번의 `INT 21h` 왕복 비용에 의존하므로 원본 DOS와 값이 다릅니다. 보정
시점과 지연 시점의 실행 backend가 다르면(interpret ↔ AOT/DBT) 실제 지연 길이가 어긋날 수
있습니다. `AH=2Ah`(Get Date) 등 다른 미구현 함수는 세 실행 파일 모두에서 호출되지 않는
라이브러리 영역에만 있어 추가하지 않았습니다.
(→ 이 AH=2Ah 판단은 아래 "2차"에서 실행 로그로 반증되어 정정되었습니다.)

---

## English

### Summary

Confirmed why `pumpit3` stopped late in initialization by cross-checking the
user-provided log against the original executable, and implemented the missing DOS
service `INT 21h AH=2Ch` (Get System Time).

### Evidence for the root cause

1. The 32-byte window in the log
   (`00 00 00 5E 5A 5B C3 31 C0 C3 53 51 52 56 B4 2C [CD] 21 ...`) matches offset
   `0xDEB31` of `build/runtime_mounts/pumpit3/PIU/PIU.EXE` byte for byte. The faulting
   EIP `0x030D3941` is the `int 21h` at offset `0xDEB41`.
2. The routine at `0xDEB3B` waits with AH=2Ch until `DH` (seconds) changes, then counts
   calls for one second and stores the count at `0x0041CD2C` — a delay-loop calibration.
3. `0x2C` was absent from the `switch (ah)` in `HandleDosInterrupt21`, so the `default`
   branch failed with `unsupported DOS INT 21h AH=0x2c`, producing the logged
   `Current execution blocker: unhandled HLE trap candidate`.
4. `B4 2C CD 21` occurrences: one in pumpit1 (`0x10BD83`) and one in pumpit2
   (`0x107A95`), both in the uncalled Watcom `_dos_gettime` region. pumpit3 has that
   library site (`0xDDED9`) plus four game-code sites (`0xDEB41` `0xDEB4B` `0xDEB5B`
   `0xDEB97`) — the reason pumpit1 and pumpit2 never exposed it.

### Changes

1. `src/platform/win32/dos/dos_int21_services.h`: declared `HandleDosGetSystemTime`.
2. `src/platform/win32/dos/dos_int21_services.cpp`: implemented
   `HandleDosGetSystemTime`, reading host local time via `GetLocalTime` and returning
   `CH:CL` = hour:minute, `DH:DL` = second:hundredths with carry cleared, preserving the
   upper halves of `ECX`/`EDX` and all of `EAX`; added `case 0x2C` to
   `HandleDosInterrupt21`.
3. `src/platform/win32/cpu_emul/instruction_emulation.cpp`: added `case 0x2C` to
   `HandleTracedDosInterrupt21`. The dispatch tables diverge by execution backend, so
   fixing only one reproduces the same stop.
4. Documentation: new design and work order for Task 397; Task 397 sections in
   `docs/analysis/interrupts-and-port-io.md` and
   `docs/analysis/current-execution-frontier.md`; AH=2Ch register contract in
   `docs/kb/important-interrupts.md`.

### Verification results

- Build: `cmake --build build --config Release --target repiu_loader_win32` succeeded
  with no new warnings (only the pre-existing C4819 code-page warnings).
- **Runtime verification is not yet done.** Per project practice the decision comes from
  a user-provided log rather than repeated game runs, so the remaining step is to confirm
  in a `repiu_host --target pumpit3` log that
  `Current execution blocker: unhandled HLE trap candidate` is gone and execution passes
  `0x030D3941`, along with the `pumpit1`/`pumpit2` regression check.

### Left unresolved

The calibration constant depends on the cost of one `INT 21h` round trip here and so
differs from original DOS; delay lengths can drift if calibration and delay run on
different backends (interpret vs AOT/DBT). Other unimplemented functions such as
`AH=2Ah` (Get Date) were not added because they appear only in uncalled library regions
of all three executables.
(→ that AH=2Ah judgment was disproved by the run log and corrected in "Round two" below.)

---

## 2차 / Round two (2026-08-02)

### 실행 결과로 확인된 것

사용자 제공 `repiu_log.txt`(13:03 실행, backend `aot-dbt`):

- `Win32 DOS AH hotspots [2C:160022 00:1 04:1 30:1]` — AH=2Ch가 160,022회 처리되어
  게스트 보정 루프가 완주했고 1차 정지 지점 `0x030D3941`은 해소됐습니다.
- 새 정지 지점은 `0x030D2CA8`, byte window `... 89 C3 B4 2A [CD] 21 66 81 E9 6C 07 ...`
  → `AH=2Ah`(Get Date)입니다.

### 1차 판단 정정

1차 설계에서 "AH=2Ah는 세 실행 파일 모두 호출되지 않는 Watcom 라이브러리 영역에만
있으므로 범위 밖"이라고 적었습니다. **틀렸습니다.** 정적 호출 관계는 다음과 같습니다.

- `0xDDE9D` = `__getdt`: `2Ah` → `2Ch` → `2Ah`(자정 넘김 확인) 순서로 호출
- `0xDDE9D`의 유일한 호출자 = `0xDB20A` = `time()`, 이어서 `0xDDF60`(INT 21h 없음)

즉 AH=2Ah와 AH=2Ch는 한 루틴이 쓰는 짝입니다. "라이브러리 영역에 있으니 호출되지
않는다"는 pumpit1/pumpit2 기준 추론이었고 pumpit3에는 적용되지 않았습니다. 도달 여부는
호출 그래프로 판정해야 합니다.

### 추가 변경

1. `src/platform/win32/dos/dos_int21_services.{h,cpp}`:
   - `HandleDosGetSystemDate` 구현. `CX` = 전체 연도, `DH` = 월, `DL` = 일,
     `AL` = 요일(0=일요일), carry clear. 게스트가 `sub cx,1900` 후 `mov ch,al`로 요일을
     사용하므로 `AL`이 실제로 필요합니다.
   - `HandleDosInterrupt21`에 `case 0x2A` 추가.
2. `src/platform/win32/cpu_emul/instruction_emulation.cpp`:
   - `HandleTracedDosInterrupt21`에 `case 0x2A` 추가.
   - `default` 분기가 `hle_message`에 `unsupported DOS INT 21h AH=0xNN`을 기록하도록
     변경. `aot-dbt`는 `enable_dos_hle`가 꺼져 있어 메시지를 남기는
     `HandleDosInterrupt21`의 `default`에 도달하지 않으므로, 두 번 모두 로그에 함수
     번호가 없어 바이트 window 대조가 필요했습니다. 다음부터는 로그가 이름을 말합니다.
3. 문서: 설계 문서에 정정 절 추가, `interrupts-and-port-io.md`,
   `current-execution-frontier.md`, `important-interrupts.md` 갱신.

### 검증 결과

- 빌드: `cmake --build build --config Release --target repiu_loader_win32` 성공,
  신규 경고 없음.
- **실행 검증 대기 중.** 다음 `repiu_host --target pumpit3` 로그에서 `0x030D2CA8` 통과와,
  이후 미구현 서비스가 나오면 `Win32 minimal execution message`에
  `unsupported DOS INT 21h AH=0xNN`이 찍히는지 확인합니다.

---

## Round two (2026-08-02)

### What the run showed

From the user-provided `repiu_log.txt` (13:03 run, `aot-dbt` backend):

- `Win32 DOS AH hotspots [2C:160022 00:1 04:1 30:1]` — AH=2Ch serviced 160,022 times,
  so the guest calibration loop completed and the first stop at `0x030D3941` is cleared.
- The new stop is `0x030D2CA8`, byte window `... 89 C3 B4 2A [CD] 21 66 81 E9 6C 07 ...`,
  i.e. `AH=2Ah` (Get Date).

### Correcting the first-round judgment

The first-round design said AH=2Ah was out of scope because it lived only in an uncalled
Watcom library region in all three executables. **That was wrong.** The static call graph:

- `0xDDE9D` = `__getdt`, calling `2Ah`, `2Ch`, then `2Ah` for the midnight-rollover check
- its only caller `0xDB20A` = `time()`, continuing into `0xDDF60` (no INT 21h)

AH=2Ah and AH=2Ch are a pair used by one routine. "It is in the library region, so it is
not called" was an inference from pumpit1/pumpit2 that did not transfer to pumpit3;
reachability has to come from the call graph.

### Additional changes

1. `src/platform/win32/dos/dos_int21_services.{h,cpp}`: implemented
   `HandleDosGetSystemDate` returning `CX` = full year, `DH` = month, `DL` = day,
   `AL` = day of week (0 = Sunday), carry cleared — the guest consumes `AL` via
   `sub cx,1900` then `mov ch,al`; added `case 0x2A` to `HandleDosInterrupt21`.
2. `src/platform/win32/cpu_emul/instruction_emulation.cpp`: added `case 0x2A` to
   `HandleTracedDosInterrupt21`, and made its `default` branch record
   `unsupported DOS INT 21h AH=0xNN` in `hle_message`. `aot-dbt` runs with
   `enable_dos_hle` off and never reaches the message-recording `default` in
   `HandleDosInterrupt21`, which is why neither log named the function and both required
   byte-window comparison. Future logs name it directly.
3. Documentation: correction sections in the design document; updates to
   `interrupts-and-port-io.md`, `current-execution-frontier.md`, and
   `important-interrupts.md`.

### Verification results

- Build: `cmake --build build --config Release --target repiu_loader_win32` succeeded with
  no new warnings.
- **Runtime verification pending.** The next `repiu_host --target pumpit3` log should show
  execution past `0x030D2CA8`, and any further missing service should appear as
  `unsupported DOS INT 21h AH=0xNN` in `Win32 minimal execution message`.
