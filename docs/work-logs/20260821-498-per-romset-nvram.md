# 20260821-498 롬셋별 NVRAM 저장 작업 로그 / Per-ROM-set NVRAM storage work log

설계: [docs/design/20260821-498-per-romset-nvram.md](../design/20260821-498-per-romset-nvram.md)
작업 지시: [docs/work-orders/20260821-498-per-romset-nvram.md](../work-orders/20260821-498-per-romset-nvram.md)

## 한국어

### 결과

93C46 EEPROM 이미지를 MAME와 같이 `nvram/<롬셋 ID>/eeprom.dat`에 저장한다. 이전에는 22개
롬셋이 작업 디렉터리의 `eeprom.dat` 하나를 공유해, 한 타이틀에서 저장한 캐비닛 설정이 다른
타이틀에도 그대로 보였다.

### 구현

| 파일 | 책임 |
|---|---|
| `include/repiu/storage/nvram_path.h`, `src/storage/nvram_path.cpp` | nvram 루트 탐색, 롬셋별 경로, 디렉터리 생성, 기존 파일 승계 |
| `include/repiu/platform/win32/eeprom_backing_path.h` | `SetEepromBackingPath()` 선언 |
| `src/platform/win32/io/port_io_emulator.cpp` | 경로 저장과 주입, 지연 생성 이후 호출 거부 |
| `src/host/win32/main.cpp` | 시작 시 1회 해석과 로그 |
| `src/tools/aot_probe/nvram_path_probe.{h,cpp}` | 검증 probe |

`src/storage/`는 게스트가 쓰는 영속 상태의 호스트 경로 정책을 두는 새 디렉터리다. 설정 파일은
사용자가 쓰고 프로그램이 읽는 반면 NVRAM은 게스트가 쓰고 읽으므로 `repiu::config`와 분리했다.

### 판단한 것

**기존 `eeprom.dat`를 승계한다.** 요구사항에 없었지만 넣었다. 이전까지 그 파일 하나가 모든
롬셋의 EEPROM 상태였으므로, 승계하지 않으면 사용자가 지금까지 맞춰 둔 캐비닛 설정이 조용히
초기화된다. 대상이 없을 때만 1회 복사하고 원본은 지우지 않는다. 여러 롬셋을 돌리면 각자 같은
시작점에서 갈라지는데, 공유 파일 하나였을 때의 상태를 각 롬셋이 물려받는 것이므로 의도한
동작이다.

**`REPIU_EEPROM_PATH`의 의미를 바꾸지 않았다.** `scripts/benchmark_*.ps1`과
`docs/guides/cd-audio-position-census.md`가 실행별 EEPROM 격리에 쓰고 있어, 전체 경로
오버라이드로 최우선 유지했다.

**파일 이름은 `eeprom.dat`를 유지했다.** MAME는 장치 태그를 파일명으로 쓰지만, 형식이 같아야
기존 이미지를 그대로 승계할 수 있다.

### 구현 중 걸린 것

**디렉터리를 먼저 만들어야 한다.** `Eeprom93c46` 생성자는 `Load()`를 호출하고 파일이 열리지
않으면 그 자리에서 `Save()`로 새 이미지를 쓴다. `Save()`는 `std::ofstream`이라 상위 디렉터리가
없으면 **조용히 실패한다.** 경로만 바꾸고 디렉터리 생성을 빼먹었다면 EEPROM이 매 실행
초기화되면서 아무 오류도 남지 않았을 것이다. 디렉터리 생성을 경로 해석 함수의 책임으로 넣고
probe에 실제 생성 여부 단정을 넣었다.

**`port_io_emulator.h`를 host에서 include할 수 없었다.** 이 헤더가 `thread_context.h`를 끌고
오는데 그 경로는 `repiu_exe`의 private include에만 있다. Task 497의 `active_jamma_bindings.h`와
같은 방식으로 `SetEepromBackingPath()` 선언만 공개 헤더로 분리했다.

### 검증

| 항목 | 결과 |
|---|---|
| Win32 x86 Debug 전체 빌드 | 통과 |
| `repiu_aot_probe --nvram-path` | checks=14 failures=0 |
| `repiu_aot_probe --romset-config` | checks=94 failures=0 |
| `repiu_aot_probe <PIU.EXE>` 전체 단정 | exit=0 |

probe가 덮는 범위는 오버라이드 우선순위, 롬셋별 경로와 디렉터리 실제 생성, 롬셋 간 경로 분리,
빈 `rom_set_id`의 기존 경로 유지, 기존 파일 승계와 내용 일치와 원본 보존, 대상이 있을 때
미승계다.

**런타임 확인은 아직 하지 않았다.** 실제 실행 후 `nvram/<롬셋>/eeprom.dat`가 생기고 캐비닛
설정이 유지되는지는 사용자 확인이 필요하다.

## English

### Result

The 93C46 EEPROM image is stored at `nvram/<rom-set-id>/eeprom.dat` the way MAME does.
Previously all 22 ROM sets shared one `eeprom.dat` in the working directory, so cabinet
settings saved under one title were what another read back.

### Implementation

| File | Responsibility |
|---|---|
| `include/repiu/storage/nvram_path.h`, `src/storage/nvram_path.cpp` | nvram root discovery, per-ROM-set path, directory creation, legacy carry-forward |
| `include/repiu/platform/win32/eeprom_backing_path.h` | The `SetEepromBackingPath()` declaration |
| `src/platform/win32/io/port_io_emulator.cpp` | Stores and injects the path, refuses a call after lazy construction |
| `src/host/win32/main.cpp` | Resolves once at startup and logs |
| `src/tools/aot_probe/nvram_path_probe.{h,cpp}` | Verification probe |

`src/storage/` is a new directory for host path policy covering persistent state the guest
writes. A config file is written by the user and read by the program, while NVRAM is
written and read by the guest, so it is kept apart from `repiu::config`.

### Judgment calls

**The existing `eeprom.dat` is carried forward.** Not requested, but included: that one
file held every ROM set's EEPROM state until now, so skipping it would silently reset the
cabinet settings a user had built up. The copy happens once, only when the destination is
missing, and the original is not deleted. Running several ROM sets makes each diverge from
the same starting point, which is the intended reading of inheriting the shared state.

**`REPIU_EEPROM_PATH` keeps its meaning.** `scripts/benchmark_*.ps1` and
`docs/guides/cd-audio-position-census.md` use it to isolate the EEPROM per run, so it stays
the highest-priority full-path override.

**The file name stays `eeprom.dat`.** MAME uses the device tag as the file name, but an
identical format is what makes carrying the existing image forward possible.

### Problems hit while implementing

**The directory has to exist first.** `Eeprom93c46`'s constructor calls `Load()` and, when
the file will not open, writes a fresh image through `Save()` right there. `Save()` uses
`std::ofstream`, which **fails silently without a parent directory.** Changing the path
without creating the directory would have reset the EEPROM on every run while reporting
nothing. Directory creation is therefore part of the path resolver's contract, and the
probe asserts the directory really appears.

**`port_io_emulator.h` could not be included from the host entry point.** It pulls in
`thread_context.h`, whose include path exists only for `repiu_exe`. Following Task 497's
`active_jamma_bindings.h`, only the `SetEepromBackingPath()` declaration moved to a public
header.

### Verification

| Item | Result |
|---|---|
| Win32 x86 Debug full build | pass |
| `repiu_aot_probe --nvram-path` | checks=14 failures=0 |
| `repiu_aot_probe --romset-config` | checks=94 failures=0 |
| `repiu_aot_probe <PIU.EXE>` full assertion set | exit=0 |

The probe covers override precedence; the per-ROM-set path and that the directory really is
created; separation between ROM sets; an empty `rom_set_id` keeping the old path; legacy
carry-forward with matching contents and a preserved original; and no carry-forward when the
destination exists.

**No runtime check yet.** Confirming that `nvram/<rom set>/eeprom.dat` appears after a real
run and that cabinet settings persist needs the user.
