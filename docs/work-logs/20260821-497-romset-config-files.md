# 20260821-497 롬셋별 설정 파일 작업 로그 / Per-ROM-set configuration file work log

설계: [docs/design/20260820-497-romset-config-files.md](../design/20260820-497-romset-config-files.md)
작업 지시: [docs/work-orders/20260820-497-romset-config-files.md](../work-orders/20260820-497-romset-config-files.md)

## 한국어

### 결과 요약

PIUIO(JAMMA) 입력의 호스트 키 매핑을 `cfg/<롬셋 ID>.ini`에서 롬셋별로 설정할 수 있게 했다.
파일이 없으면 주석 처리된 템플릿으로 생성하고, 조합키를 지원하며, 설정 가능한 키 이름 목록을
문서와 생성 파일 주석 양쪽에 둔다.

전체 빌드와 probe가 통과했다. `romset_config_probe`는 94개 단정을 실행하며 실패 0이다.

### 구현한 것

| 파일 | 책임 |
|---|---|
| `include/repiu/config/config_name.h` | 설정 식별자 비교 규칙(대소문자·언더스코어 무시), header-only |
| `include/repiu/config/ini_document.h`, `src/config/ini_document.cpp` | INI 파싱. 실패하지 않고 경고만 반환 |
| `include/repiu/config/romset_config.h`, `src/config/romset_config.cpp` | cfg 탐색, 부모 사슬, 첫 실행 생성 |
| `include/repiu/config/romset_config_template.h`, `src/config/romset_config_template.cpp` | 생성 파일 본문 렌더링 |
| `include/repiu/input/jamma_input_key.h`, `src/input/jamma_input_key.cpp` | `JammaInputKey` 열거와 설정 이름 |
| `include/repiu/input/host_key_names.h`, `src/input/host_key_names.cpp` | 이름 ↔ `SDL_Keycode` 표 |
| `include/repiu/input/host_key_binding.h`, `src/input/host_key_binding.cpp` | 조합키 파싱, 요구·금지 마스크 |
| `include/repiu/input/jamma_input_bindings.h`, `src/input/jamma_input_bindings.cpp` | 포트 비트 표, 기본값, `[Input]` 적용 |
| `include/repiu/platform/win32/active_jamma_bindings.h`, `src/platform/win32/input/active_jamma_bindings.cpp` | 이번 실행의 바인딩 저장소 |
| `src/platform/win32/input/win32_host_key_translation.{h,cpp}` | `SDL_Keycode` → 가상키, 수식키 상태 |
| `src/tools/aot_probe/romset_config_probe.{h,cpp}` | 검증 probe |

수정한 곳은 `port_io_emulator.cpp`(스캔·로그), `jamma_input_timeline.cpp`(초기 상태 캡처),
`glide_opengl_backend.cpp`(SDL event 경로), `main.cpp`(로드 통합), `CMakeLists.txt`,
`.gitignore`다.

### 설계에서 바뀐 것

**`HostKeyId` 열거를 없애고 `SDL_Keycode`를 정식 타입으로 썼다.** 검토 중 사용자가 SDLK 사용
가능성을 물었고, 조사 결과 표 3개가 1개로 줄어드는 단순화였다. 다만 SDL의 **이름 문자열**은
쓰지 않는다. `SDL_GetScancodeName` 헤더가 "unsuitable for creating a stable cross-platform
two-way mapping between strings and scancodes"라고 명시하고, 이름에 공백이 들어가며
(`Keypad 7`, `Page Up`), 조회 함수가 `not thread safe`다. 이름 표는 프로젝트가 유지하되 대상만
`SDL_Keycode`로 두었다.

**생성 파일을 주석 처리 템플릿으로 바꿨다.** 활성 항목으로 쓰면 자식 파일이 14개 키를 전부
덮어써 부모 레이어링이 죽는다는 것을 계획 단계에서 발견했다. 주석 처리하면 레이어링이 유지되고
"생성이 동작을 바꾸지 않는다"가 구조적으로 성립한다.

**빈 값 = 입력 끄기 (한 번 잘못 바꿨다가 되돌림).** 최초 구현은 `TEST =`를 그 입력을 끄는
것으로 해석했고, 이것이 맞았다. 검토 중 "값이 없으면 그 키를 무시하자"는 지시를 "그 설정
줄을 무시한다"로 잘못 읽어 항목 무시로 바꿨다가(7894367), 사용자가 TEST/SERVICE를 비웠는데도
여전히 동작한다고 알려와 원래 의미로 되돌렸다. "그 키를 무시"는 **그 입력을 처리하지 않는다**는
뜻이었다.

현재 규칙은 항목이 그 입력의 별칭 목록을 통째로 교체한다는 것이고, 빈 값은 "이 입력에 키가
없다"는 완전한 지정이다 (설계 4.1절). probe에 별칭 수가 0인지와 가상키까지 사라지는지를
확인하는 단정을 넣었다.

**`repiu_exe`의 SDL3 링크를 PRIVATE에서 PUBLIC으로 바꿨다.** public 헤더가 `SDL_Keycode`와
`SDL_Keymod`를 노출하므로 SDL3 include 경로가 이 라이브러리의 인터페이스에 속한다. PRIVATE인
채로는 `repiu` 실행 파일이 빌드되지 않았다.

### 구현 중 발견해 고친 것

**키 릴리스가 조합키에서 멈출 수 있었다.** SDL key up event의 `event.key.mod`는 **뗄 때의**
수식키 상태다. `TEST = Ctrl+F1`에서 Ctrl+F1을 누른 뒤 Ctrl을 먼저 놓고 F1을 놓으면, key up의
mod가 0이라 `Ctrl+F1` 별칭과 일치하지 않아 릴리스가 기록되지 않는다. TEST가 실행 내내 눌린 채로
남는다. 릴리스는 수식키를 따지지 않고 **해당 키코드에 걸린 모든 입력**을 해제하도록 했다
(`JammaInputMaskForKeycode`). 누르지 않은 입력을 해제하는 것은 타임라인에서 무해하다.

**이름 표의 첫 구현이 dangling `string_view`를 만들었다.** 문자·숫자·기능키·숫자패드가 SDL
keycode 공간에서 연속이라 시작 시 생성하도록 짰는데, 생성된 이름의 저장소를 구조체가 들고
있어 값 반환 시 모든 `string_view`가 매달렸다. NRVO는 보장이 아니다. 리터럴 기반 `constexpr`
표로 바꿔 수명 문제 자체를 없앴다.

**probe의 회귀 가드가 자기참조였다.** "설정 파일이 없으면 기본값과 같다"를
`DefaultJammaBindings()`와 비교하면 로더의 자기 일관성만 증명한다. 구현 이전 `ScanJammaPort8`의
하드코딩 매핑(포트·비트·가상키 집합)을 직접 전사한 표를 넣고 그것과 대조하도록 고쳤다.
2P의 숫자패드/편집키 쌍과 그 순서까지 검증한다.

### 검증

| 항목 | 결과 |
|---|---|
| Win32 x86 Debug 빌드 (`repiu_exe`, `repiu`, `repiu_aot_probe`) | 통과 |
| `repiu_aot_probe --romset-config` | checks=94 failures=0 |
| `repiu_aot_probe <PIU.EXE>` 전체 단정 | exit=0 |
| 생성 파일 육안 확인 | 통과. 부모 값(`P1_CENTER = M`, `TEST = F9`)이 주석으로 기록됨 |

probe가 덮는 범위는 INI 파싱, 별칭·빈 값 무시·잘못된 이름, 이름 표 건전성(중복·가상키 누락·왕복),
조합키 파싱과 거부 형태, 수식키 판정과 경쟁 규칙과 lock 비트 배제, 역사적 기본 매핑 전사 대조,
레이어링, 생성 파일 왕복과 미덮어쓰기, 부모 사슬 순서와 순환 방지다.

**런타임 스모크는 수행하지 않았다.** 사용자 메모리에 "크래시 조사는 게임 반복 실행 말고 로그
분석으로, 실행이 필요하면 먼저 확인"이 남아 있어 실행 전에 확인이 필요하다. 생성 경로 자체는
probe가 임시 디렉터리에 실제 파일을 쓰고 되읽어 검증했다. 다만 **게임이 실제로 뜬 상태에서
설정한 키가 발판으로 들어가는지는 아직 확인하지 않았다.**

### 남은 항목

* 런타임 스모크(사용자 확인 후): `repiu.exe pumpit1` 실행 → `cfg/pumpit1.ini` 생성 확인 →
  한 항목 주석 해제 후 재실행 → `[repiu-input]` 로그에서 새 키가 잡히는지 확인.
* `COIN2`와 캐비닛 포트 `0x02AB`: 비트 미확정으로 제외. 별도 작업에서 게임이 그 포트를 읽는지
  부터 확인해야 한다.
* `[Video]`, `[Audio]` 같은 다른 섹션은 아직 없다. 현재는 인식하지 않는 섹션을 조용히 무시하며,
  이는 나중 버전용 파일이 이전 빌드를 막지 않게 하려는 의도다.

## English

### Summary

PIUIO (JAMMA) host key mapping is now configurable per ROM set through
`cfg/<rom-set-id>.ini`. A missing file is generated as a commented-out template, key
combinations are supported, and the configurable key names are published in both the guide
and the generated file's comments.

The full build and probe suite pass. `romset_config_probe` runs 94 assertions with zero
failures.

### What was built

| File | Responsibility |
|---|---|
| `include/repiu/config/config_name.h` | The config identifier comparison rule (case and underscores ignored), header-only |
| `include/repiu/config/ini_document.h`, `src/config/ini_document.cpp` | INI parsing; never fails, returns warnings |
| `include/repiu/config/romset_config.h`, `src/config/romset_config.cpp` | cfg discovery, parent chain, first-run generation |
| `include/repiu/config/romset_config_template.h`, `src/config/romset_config_template.cpp` | Renders the generated file body |
| `include/repiu/input/jamma_input_key.h`, `src/input/jamma_input_key.cpp` | The `JammaInputKey` enum and its config names |
| `include/repiu/input/host_key_names.h`, `src/input/host_key_names.cpp` | Name to `SDL_Keycode` table |
| `include/repiu/input/host_key_binding.h`, `src/input/host_key_binding.cpp` | Combination parsing, required/forbidden masks |
| `include/repiu/input/jamma_input_bindings.h`, `src/input/jamma_input_bindings.cpp` | Port bit table, defaults, `[Input]` application |
| `include/repiu/platform/win32/active_jamma_bindings.h`, `src/platform/win32/input/active_jamma_bindings.cpp` | The bindings store for this run |
| `src/platform/win32/input/win32_host_key_translation.{h,cpp}` | `SDL_Keycode` to virtual key, modifier state |
| `src/tools/aot_probe/romset_config_probe.{h,cpp}` | Verification probe |

Modified: `port_io_emulator.cpp` (scan and log), `jamma_input_timeline.cpp` (initial state
capture), `glide_opengl_backend.cpp` (SDL event path), `main.cpp` (load integration),
`CMakeLists.txt`, and `.gitignore`.

### Changes from the design

**The `HostKeyId` enum was dropped in favor of `SDL_Keycode` as the canonical type.** The
user asked during review whether SDLK could be used, and the investigation showed it
collapses three tables into one. SDL's own **name strings** are still not used:
`SDL_GetScancodeName`'s header states they are "unsuitable for creating a stable
cross-platform two-way mapping between strings and scancodes", the names contain spaces
(`Keypad 7`, `Page Up`), and the lookups are documented as not thread safe. The project
keeps its own name table, targeting `SDL_Keycode`.

**The generated file became a commented-out template.** Planning surfaced that active
entries would have the child file override all fourteen inputs and kill parent layering.
Commenting them keeps layering alive and makes "generation does not change behavior" true
by construction.

**An empty value turns the input off (changed wrongly once, then reverted).** The first
implementation read `TEST =` as turning that input off, which was correct. During review a
instruction to "ignore that key when there is no value" was misread as "ignore that config
line", and the behavior was changed to skip the entry (7894367). The user then reported
that TEST and SERVICE still worked after emptying them, and it was reverted: "ignore that
key" meant **do not handle that input**.

The rule now is that an entry replaces the input's whole alias list, and an empty value is
the complete instruction "no key drives this" (design section 4.1). The probe asserts both
that the alias count is zero and that no virtual key survives resolution.

**`repiu_exe`'s SDL3 link changed from PRIVATE to PUBLIC.** Its public headers expose
`SDL_Keycode` and `SDL_Keymod`, so SDL3's include directories are part of the library's
interface. The `repiu` executable did not build while it stayed PRIVATE.

### Problems found and fixed while implementing

**Key release could get stuck on a combination.** An SDL key up event's `event.key.mod` is
the modifier state **at release time**. With `TEST = Ctrl+F1`, pressing Ctrl+F1 then
releasing Ctrl before F1 leaves the key up with mod 0, which no longer satisfies the
`Ctrl+F1` alias, so the release is never recorded and TEST stays held for the rest of the
run. Releases now ignore modifiers and clear **every input bound to that keycode**
(`JammaInputMaskForKeycode`); releasing an input that was not pressed is a no-op in the
timeline.

**The first name table implementation created dangling `string_view`s.** Letters, digits,
function keys, and keypad digits are contiguous in the SDL keycode space, so the table was
generated at startup — but the generated spellings lived in the same struct, and returning
it by value left every view dangling. NRVO is not a guarantee. Replacing it with a literal
`constexpr` table removed the lifetime question entirely.

**The probe's regression guard was self-referential.** Comparing "no config file equals the
defaults" against `DefaultJammaBindings()` only proves the loader is self-consistent. It
now compares against a direct transcription of the pre-change hardcoded mapping in
`ScanJammaPort8` — port, bit, and the exact virtual key set — including the P2 keypad and
editing key pairs and their order.

### Verification

| Item | Result |
|---|---|
| Win32 x86 Debug build (`repiu_exe`, `repiu`, `repiu_aot_probe`) | pass |
| `repiu_aot_probe --romset-config` | checks=94 failures=0 |
| `repiu_aot_probe <PIU.EXE>` full assertion set | exit=0 |
| Generated file inspected by eye | pass; parent values (`P1_CENTER = M`, `TEST = F9`) recorded as comments |

The probe covers INI parsing; alias lists, ignored empty values, and invalid names; name table health
(duplicates, missing virtual keys, round trip); combination parsing and the rejected
shapes; modifier matching, the contention rule, and lock-bit exclusion; the transcribed
historical default mapping; layering; generated-file round trip and no-overwrite; and
parent chain ordering and cycle bounding.

**No runtime smoke test was performed.** A stored user preference says crash investigation
should use supplied logs rather than repeated game runs, and to confirm before running, so
this needs the user's go-ahead. The generation path itself was verified by the probe
writing real files to a temporary directory and reading them back. What remains unverified
is **whether a configured key reaches the stage panel in a live game session.**

### Remaining items

* Runtime smoke, after user confirmation: run `repiu.exe pumpit1`, confirm
  `cfg/pumpit1.ini` appears, uncomment one entry, rerun, and confirm the new key shows up
  in the `[repiu-input]` log.
* `COIN2` and cabinet port `0x02AB`: excluded because their bits are unidentified. A
  separate task must first confirm whether the game reads that port.
* Other sections such as `[Video]` and `[Audio]` do not exist yet. Unrecognized sections
  are silently ignored so a file written for a later version does not stop an earlier
  build.
