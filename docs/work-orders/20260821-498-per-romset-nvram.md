# 20260821-498 롬셋별 NVRAM 저장 작업 지시서 / Per-ROM-set NVRAM storage work order

설계: [docs/design/20260821-498-per-romset-nvram.md](../design/20260821-498-per-romset-nvram.md)

## 한국어

### 목적

93C46 EEPROM 이미지를 MAME처럼 `nvram/<롬셋 ID>/eeprom.dat`에 저장한다. 지금은 모든 롬셋이
작업 디렉터리의 `eeprom.dat` 하나를 공유해 캐비닛 설정이 타이틀 간에 섞인다.

### 작업 순서

1. **경로 해석** — `include/repiu/storage/nvram_path.h`, `src/storage/nvram_path.cpp`
   - `REPIU_EEPROM_PATH` 전체 경로 오버라이드를 최우선으로 유지
   - nvram 루트 탐색: `REPIU_NVRAM_DIR` → 작업 디렉터리 `nvram` → 실행 파일 옆 `nvram`,
     없으면 작업 디렉터리 아래 생성
   - `rom_set_id`가 비면 기존 `eeprom.dat` 경로 유지
   - **디렉터리를 먼저 만든다.** `Eeprom93c46`가 파일이 없으면 생성자에서 바로 쓰는데,
     상위 디렉터리가 없으면 조용히 실패한다 (설계 5절)
   - 기존 `eeprom.dat` 승계: 대상이 없을 때만 1회 복사, 원본 유지, 실패 시 경고 후 계속
2. **주입 지점** — `src/platform/win32/io/port_io_emulator.{h,cpp}`
   - `SetEepromBackingPath()` 추가, `EepromBackingPath()`가 저장된 값을 쓰도록 변경
   - `g_eeprom` 생성 이후의 호출은 무시하고 경고
3. **통합** — `src/host/win32/main.cpp`에서 대상 프로파일 확정 직후 해석하고, 결정된 경로와
   승계 여부, 경고를 로그로 남긴다
4. **빌드 등록** — `CMakeLists.txt`에 새 소스와 probe 추가
5. **검증 probe** — `src/tools/aot_probe/nvram_path_probe.{h,cpp}` 추가 및 등록
   (설계 7절 6개 항목)
6. **`.gitignore`** — `nvram/` 추가. 기존 `eeprom.dat` 항목은 승계 원본이므로 유지
7. **문서 갱신** — `ARCHITECTURE.md`, `docs/analysis/piu-io-port-specification.md`,
   `docs/guides/romset-config-files.md`(경로 안내), 작업 로그
8. **커밋** — 작업 브랜치에 커밋

### 범위

EEPROM 내용의 해석, 초기값, 93C46 상태 머신은 바꾸지 않는다. 파일 이름도 `eeprom.dat`를
유지한다 — 형식이 같아야 기존 이미지를 승계할 수 있다. 다른 영속 상태(세이브 등)는 이번
범위가 아니다.

### 유지해야 할 불변식

- `REPIU_EEPROM_PATH`의 기존 의미가 바뀌지 않아야 한다. 벤치마크 스크립트와 측정 가이드가
  실행별 EEPROM 격리에 쓰고 있다
- EEPROM 파일을 쓰기 전에 상위 디렉터리가 존재해야 한다
- 기존 `eeprom.dat`의 내용이 유실되지 않아야 한다. 원본은 지우지 않는다
- 승계는 대상이 없을 때만 일어나야 한다. 롬셋별 파일이 생긴 뒤에는 그것이 정본이다
- 경로 해석 실패가 실행을 막지 않아야 한다 (경고 후 계속)

### 최소 검증

- 신규 `nvram_path_probe` 통과
- 전체 `repiu_aot_probe` 통과
- Win32 x86 Debug 빌드 통과
- 런타임 확인: 실행 후 `nvram/<롬셋>/eeprom.dat` 생성 확인 (사용자 확인 후)

## English

### Objective

Store the 93C46 EEPROM image at `nvram/<rom-set-id>/eeprom.dat` the way MAME does. Today
every ROM set shares one `eeprom.dat` in the working directory, so cabinet settings bleed
between titles.

### Work items

1. **Path resolution** — `include/repiu/storage/nvram_path.h`, `src/storage/nvram_path.cpp`
   - Keep `REPIU_EEPROM_PATH` as the highest-priority full-path override
   - nvram root discovery: `REPIU_NVRAM_DIR`, then `nvram` under the working directory,
     then `nvram` next to the executable; create the working-directory one if none exists
   - An empty `rom_set_id` keeps the existing `eeprom.dat` path
   - **Create the directory first.** `Eeprom93c46` writes from its constructor when the
     file is absent, and that write fails silently without a parent directory (design
     section 5)
   - Carry the legacy `eeprom.dat` forward: copy once only when the destination is
     missing, keep the original, warn and continue on failure
2. **Injection point** — `src/platform/win32/io/port_io_emulator.{h,cpp}`
   - Add `SetEepromBackingPath()` and have `EepromBackingPath()` use the stored value
   - Ignore a call made after `g_eeprom` exists, with a warning
3. **Integration** — resolve in `src/host/win32/main.cpp` right after the target profile is
   settled, logging the chosen path, whether a legacy file was carried forward, and warnings
4. **Build registration** — add the new sources and probe to `CMakeLists.txt`
5. **Verification probe** — add and register
   `src/tools/aot_probe/nvram_path_probe.{h,cpp}` (the six items in design section 7)
6. **`.gitignore`** — add `nvram/`; keep the existing `eeprom.dat` entry, which is now the
   carry-forward source
7. **Documentation** — update `ARCHITECTURE.md`,
   `docs/analysis/piu-io-port-specification.md`, `docs/guides/romset-config-files.md` (path
   guidance), and the work log
8. **Commit** — commit on the task branch

### Scope

EEPROM content interpretation, initial values, and the 93C46 state machine are unchanged.
The file name stays `eeprom.dat` because an identical format is what makes carrying the
existing image forward possible. Other persistent state such as saves is out of scope.

### Invariants to preserve

- `REPIU_EEPROM_PATH` must keep its existing meaning; benchmark scripts and measurement
  guides use it to isolate the EEPROM per run
- The parent directory must exist before the EEPROM file is written
- The contents of an existing `eeprom.dat` must not be lost, and the original is not deleted
- Carry-forward happens only when the destination is missing; once the per-ROM-set file
  exists it is authoritative
- A path resolution failure must not stop the run (warn and continue)

### Minimum verification

- The new `nvram_path_probe` passes
- The full `repiu_aot_probe` suite passes
- The Win32 x86 Debug build passes
- Runtime check: confirm `nvram/<rom set>/eeprom.dat` appears after a run (after user
  confirmation)
