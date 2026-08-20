# 20260820-497 롬셋별 설정 파일 작업 지시서 / Per-ROM-set configuration file work order

설계: [docs/design/20260820-497-romset-config-files.md](../design/20260820-497-romset-config-files.md)

## 한국어

### 목적

`cfg/<롬셋 ID>.ini` 파일을 읽어 PIUIO(JAMMA) 입력의 호스트 키 매핑을 롬셋별로 설정할 수
있게 한다. 파일이 없으면 기본값으로 생성하고, 조합키를 지원하며, 설정 가능한 키 이름
목록을 문서와 생성 파일 주석으로 제공한다. 이후 다른 설정 항목을 섹션 단위로 추가할 수
있는 구조를 함께 세운다.

### 작업 순서 (완료)

1. **INI 파서** — `include/repiu/config/ini_document.h`, `src/config/ini_document.cpp`
   - 주석(`;`, `#`), 섹션, `key = value`, 공백 제거, 따옴표 벗기기, 중복 키 최종값 채택
   - 파싱 실패 없음. 인식 못 한 줄은 경고 목록으로 반환
2. **호스트 키 이름 표** — `include/repiu/input/host_key_names.h`,
   `src/input/host_key_names.cpp`
   - 이름 ↔ `SDL_Keycode` 표, 대소문자·언더스코어 무시 조회
   - SDL 자체 이름(`SDL_GetKeyFromName`)은 쓰지 않는다. 헤더가 안정적 양방향 매핑에
     부적합하다고 명시하고, 이름에 공백이 들어가며, `not thread safe`다 (설계 7.2절)
   - 분류(문자/숫자/기능키/숫자패드/이동·편집/기타/수식키) 메타데이터를 함께 둔다.
     생성 파일 주석과 문서가 이 분류를 사용한다
3. **조합키 파싱** — `include/repiu/input/host_key_binding.h`,
   `src/input/host_key_binding.cpp`
   - `Ctrl+Shift+F2` 문법, 마지막 토큰이 기본 키, 앞 토큰은 수식키만 허용
   - `Ctrl`/`Shift`/`Alt`는 좌우 무관(`SDL_KMOD_CTRL` 등이 그대로 마스크), 사이드
     지정 이름도 지원
   - 별칭마다 요구 `SDL_Keymod` 마스크 목록과 금지 마스크로 환산
   - 비교 전에 `SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT`로 마스킹해 NumLock,
     CapsLock 같은 lock 비트를 배제한다
4. **JAMMA 바인딩** — `include/repiu/input/jamma_input_bindings.h`,
   `src/input/jamma_input_bindings.cpp`
   - 14개 액션 정의, 내장 기본값, `[Input]` 섹션 적용, 쉼표 별칭 목록
   - 빈 값은 그 입력을 끈다. 항목은 별칭 목록을 통째로 교체하므로 빈 값이 곧
     "이 입력에 키가 없다"는 지정이다 (설계 4.1절)
   - 같은 기본 키를 두고 경쟁이 있을 때 수식키 없는 별칭의 금지 마스크 파생
   - `any_binding_uses_modifiers` 플래그 계산
   - `ResolvedJammaBindings`는 고정 배열, 동적 할당·문자열 없음
5. **설정 로더** — `include/repiu/config/romset_config.h`, `src/config/romset_config.cpp`
   - `cfg` 디렉터리 탐색(`REPIU_CFG_DIR` → CWD → 실행 파일 옆), 없으면 CWD 아래 생성
   - `parent_rom_set_id` 사슬 최대 4단계 레이어링, 키 단위 덮어쓰기
   - 첫 실행 생성: 해석 결과를 **주석 처리 템플릿**으로 임시 파일에 CRLF로 쓰고 rename,
     기존 파일은 미덮어쓰기, 실패 시 경고 후 계속,
     `REPIU_CFG_WRITE_DEFAULT=0`으로 비활성
   - 부모 파일은 생성하지 않는다
   - `rom_set_id`가 빈 프로파일은 읽기·생성 모두 건너뜀
6. **기본 파일 렌더링** — `include/repiu/config/romset_config_template.h`,
   `src/config/romset_config_template.cpp`
   - 문법·수식키·키 이름 목록 주석 블록과 `[Input]` 본문 생성
   - **입력 항목은 전부 선행 `;`를 붙여 주석 처리**한다. `[Input]` 섹션 헤더는 주석
     처리하지 않는다 (설계 5.1, 12절)
   - 헤더 주석에 "주석을 풀면 이 롬셋만 덮어쓴다", "전체에 적용하려면 공용 파일
     `cfg/pumpitup.ini`를 만든다"를 안내한다
   - 파일시스템을 건드리지 않는 순수 문자열 생성으로 유지 (probe 검사용)
7. **플랫폼 변환** — `src/platform/win32/input/win32_host_key_translation.{h,cpp}`
   - `SDL_Keycode` → Win32 가상키 (남는 변환표는 이것 하나뿐)
   - 6개 수식키의 `GetAsyncKeyState` 결과를 `SDL_Keymod` 값으로 조립
   - SDL 이벤트 경로는 `event.key.key`, `event.key.mod`와 직접 비교하므로 변환 없음
8. **열거 이름 정정** — `JammaInputKey`를 실제 비트 의미에 맞게 rename (순서 불변)
   - `jamma_input_timeline.h/.cpp`, `port_io_emulator.cpp`, `glide_opengl_backend.cpp`,
     `jamma_input_timeline_probe.cpp`
9. **소비 지점 연결**
   - `ScanJammaPort8`: 하드코딩 `is_pressed` 호출을 포트별 바인딩 표 순회로 교체하고,
     로그용 비트 이름 표를 같은 표로 통합
   - 수식키 상태는 `any_binding_uses_modifiers`가 참일 때만, 스냅샷 갱신마다 한 번 읽어
     모든 별칭이 공유
   - `CaptureCurrentJammaPressedMask`: 하드코딩 mapping 배열 제거
   - `TranslateJammaInputKeyValue`: `switch` 대신 바인딩 표 조회로 교체하고
     `event.key.mod`로 수식키 판정 (기존 `static_assert` 검증은 probe 단언으로 이전)
10. **통합** — `src/host/win32/main.cpp`에서 대상 프로파일 확정 직후 1회 로드하고,
    적용된 파일 경로, 생성 여부, 경고를 로그로 남긴다
11. **빌드 등록** — `CMakeLists.txt`에 새 소스와 probe 추가
12. **검증 probe** — `src/tools/aot_probe/romset_config_probe.cpp` 추가 및 등록
13. **문서 갱신**
    - 신규 `docs/guides/romset-config-files.md` (파일 위치, 첫 실행 생성, 문법,
      조합키 규칙, 전체 키 목록, 입력 이름 표, 문제 해결)
    - `ARCHITECTURE.md`, `docs/analysis/piu-io-port-specification.md`,
      `README.md`(cfg 사용법 링크), 작업 로그
14. **`.gitignore`** — `cfg/`를 무시 목록에 추가
15. **커밋** — 작업 브랜치에 커밋 (원격 push는 하지 않음)

### 범위

원본 실행 파일, 게임 로직, 포트 비트 의미, 타임라인 재생 구조는 바꾸지 않는다. 호스트 키
목록의 출처만 하드코딩에서 설정 파일로 옮긴다. `COIN2`와 캐비닛 포트 `0x02AB`는
에뮬레이터가 아직 스캔하지 않으므로 제외한다. 게임패드·조이스틱 입력, 게임 내 설정 저장,
`[Input]` 외 섹션은 이번 범위가 아니다.

### 유지해야 할 불변식

- 설정 파일이 없으면 결과 바인딩이 현재 하드코딩 값과 정확히 같아야 한다
- 기본값 파일 생성이 동작을 바꾸지 않아야 한다. 생성 파일의 입력 항목은 전부 주석
  처리되므로, 생성 직후 되읽었을 때 활성 항목이 하나도 없어야 한다
- 자식 파일을 생성한 뒤에도 부모 파일의 값이 계속 적용되어야 한다 (설계 5.1절)
- 기존 설정 파일을 절대 덮어쓰지 않아야 한다
- 수식키를 쓰지 않는 설정에서는 수식키 조회가 아예 일어나지 않아야 한다 (Task 403)
- 스캔 경로에 문자열 비교·맵 조회·할당이 없어야 한다 (Task 403).
  `SDL_Keycode` → VK 변환도 로드 시점에 끝나야 한다
- 수식키 없는 별칭은 경쟁이 없는 한 수식키를 따지지 않아야 한다 (회귀 방지)
- NumLock, CapsLock, ScrollLock 상태가 어떤 바인딩에도 영향을 주지 않아야 한다.
  2P 기본값이 NumLock OFF 숫자패드를 쓰므로 특히 중요하다
- `JammaInputKey` 열거 **순서**는 바뀌지 않아야 한다 (`JammaInputKeyMask` 비트 위치)
- 설정 파일 오류와 파일 쓰기 실패가 실행을 막지 않아야 한다 (경고 후 계속)
- 빈 값으로 끈 입력은 폴링 경로와 창 이벤트 경로 양쪽에서 완전히 사라져야 한다.
  가상키도 남지 않아야 한다

### 최소 검증

- 신규 `romset_config_probe` 통과 (설계 16절 9개 항목)
- 전체 `repiu_aot_probe` 통과
- Win32 x86 Debug 빌드 통과 (`build/win32_x86_debug`)
- 런타임 스모크는 사용자 확인 후에만 수행하고, 수행하지 않으면 이유를 작업 로그에 남긴다

## English

### Objective

Read `cfg/<rom-set-id>.ini` so PIUIO (JAMMA) host key mapping can be configured per ROM
set. Generate the file with defaults when it is missing, support key combinations, and
publish the list of configurable key names through both documentation and the generated
file's comments. Establish a structure that lets later settings be added as further
sections.

### Work items (complete)

1. **INI parser** — `include/repiu/config/ini_document.h`, `src/config/ini_document.cpp`
   - Comments (`;`, `#`), sections, `key = value`, whitespace trimming, quote
     unwrapping, last-duplicate-wins
   - Never fails; unrecognized lines are returned as warnings
2. **Host key name table** — `include/repiu/input/host_key_names.h`,
   `src/input/host_key_names.cpp`
   - Name to `SDL_Keycode` table, lookup ignoring case and underscores
   - Do not use SDL's own names (`SDL_GetKeyFromName`): the header states they are
     unsuitable for a stable two-way mapping, they contain spaces, and the lookup is
     `not thread safe` (design section 7.2)
   - Carry group metadata (letters, digits, function, keypad, navigation/editing, other,
     modifiers) used by both the generated comments and the guide
3. **Combination parsing** — `include/repiu/input/host_key_binding.h`,
   `src/input/host_key_binding.cpp`
   - `Ctrl+Shift+F2` syntax; last token is the base key, preceding tokens must be
     modifiers
   - `Ctrl`/`Shift`/`Alt` accept either side (`SDL_KMOD_CTRL` and friends are the masks
     directly); sided names are also supported
   - Reduce each alias to a required `SDL_Keymod` mask list plus a forbidden mask
   - Mask with `SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT` before comparing so lock
     bits such as NumLock and CapsLock are excluded
4. **JAMMA bindings** — `include/repiu/input/jamma_input_bindings.h`,
   `src/input/jamma_input_bindings.cpp`
   - The 14 action definitions, built-in defaults, `[Input]` application, comma-separated
     alias lists
   - An empty value turns the input off: an entry replaces the whole alias list, so an
     empty value is the instruction "no key drives this" (design section 4.1)
   - Derive the forbidden mask for unmodified aliases that contend over a base key
   - Compute the `any_binding_uses_modifiers` flag
   - `ResolvedJammaBindings` is a fixed array with no allocation and no strings
5. **Config loader** — `include/repiu/config/romset_config.h`,
   `src/config/romset_config.cpp`
   - `cfg` discovery (`REPIU_CFG_DIR`, then CWD, then next to the executable), creating
     the CWD-relative one if none exists
   - `parent_rom_set_id` chain layering capped at four levels, overriding per key
   - First-run generation: write resolved values as a **commented-out template** to a
     temporary file with CRLF and rename, never overwrite an existing file, warn and
     continue on failure, disabled by `REPIU_CFG_WRITE_DEFAULT=0`
   - Never generate parent files
   - Skip both reading and generation for profiles with an empty `rom_set_id`
6. **Default file rendering** — `include/repiu/config/romset_config_template.h`,
   `src/config/romset_config_template.cpp`
   - Produce the syntax, modifier, and key-name comment block plus the `[Input]` body
   - **Prefix every input entry with `;`** so it is commented out; leave the `[Input]`
     section header uncommented (design sections 5.1 and 12)
   - Have the header comment explain that uncommenting overrides for this ROM set only,
     and that a shared `cfg/pumpitup.ini` applies to every set
   - Keep it pure string generation with no filesystem access, so probes can inspect it
7. **Platform translation** — `src/platform/win32/input/win32_host_key_translation.{h,cpp}`
   - `SDL_Keycode` to Win32 virtual key — the only translation table that remains
   - Assemble an `SDL_Keymod` value from `GetAsyncKeyState` on the six modifier keys
   - The SDL event path compares `event.key.key` and `event.key.mod` directly, with no
     translation
8. **Enum rename** — align `JammaInputKey` names with actual bit meanings, order unchanged
   - `jamma_input_timeline.h/.cpp`, `port_io_emulator.cpp`, `glide_opengl_backend.cpp`,
     `jamma_input_timeline_probe.cpp`
9. **Wire the consumers**
   - `ScanJammaPort8`: replace hardcoded `is_pressed` calls with a walk over the
     per-port binding table, and merge the logging bit-name table into that same table
   - Read modifier state only when `any_binding_uses_modifiers` is true, once per
     snapshot refresh, shared by every alias
   - `CaptureCurrentJammaPressedMask`: drop its hardcoded mapping array
   - `TranslateJammaInputKeyValue`: replace the `switch` with a binding-table lookup and
     judge modifiers from `event.key.mod`, moving the existing `static_assert` checks
     into probe assertions
10. **Integration** — load once in `src/host/win32/main.cpp` right after the target
    profile resolves, logging the applied file paths, whether a file was generated, and
    any warnings
11. **Build registration** — add the new sources and probe to `CMakeLists.txt`
12. **Verification probe** — add and register
    `src/tools/aot_probe/romset_config_probe.cpp`
13. **Documentation**
    - New `docs/guides/romset-config-files.md` (file location, first-run generation,
      syntax, combination rules, full key list, input name table, troubleshooting)
    - Update `ARCHITECTURE.md`, `docs/analysis/piu-io-port-specification.md`,
      `README.md` (link the cfg usage), and the work log
14. **`.gitignore`** — add `cfg/` to the ignore list
15. **Commit** — commit on the task branch; do not push to the remote

### Scope

Do not change the original executable, game logic, port bit semantics, or the timeline
replay structure. Only the source of the host key list moves from hardcoded values to a
config file. `COIN2` and the cabinet port `0x02AB` are excluded because the emulator does
not scan them yet. Gamepad and joystick input, in-game settings saving, and sections
other than `[Input]` are out of scope.

### Invariants to preserve

- With no config file, resolved bindings must equal today's hardcoded values exactly
- Generating the default file must not change behavior: every input entry it writes is
  commented out, so reading it straight back must yield no active entry
- A parent file's values must keep applying after the child file has been generated
  (design section 5.1)
- An existing config file must never be overwritten
- A configuration that uses no modifiers must perform no modifier query at all (Task 403)
- The scan path must contain no string comparison, map lookup, or allocation (Task 403),
  and the `SDL_Keycode` to VK conversion must finish at load time
- An unmodified alias must ignore modifiers unless it contends over a base key
  (regression guard)
- NumLock, CapsLock, and ScrollLock state must not affect any binding — especially
  important because the P2 defaults rely on a NumLock-off keypad
- `JammaInputKey` enum **order** must not change (`JammaInputKeyMask` bit positions)
- Neither a config file error nor a failed write may stop the run (warn and continue)
- An input turned off by an empty value must disappear from both the polling path and the
  window event path, down to having no virtual key left

### Minimum verification

- The new `romset_config_probe` passes (the nine items in design section 16)
- The full `repiu_aot_probe` suite passes
- The Win32 x86 Debug build passes (`build/win32_x86_debug`)
- A runtime smoke test is run only after user confirmation; if skipped, the reason is
  recorded in the work log
