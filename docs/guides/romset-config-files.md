# 롬셋별 설정 파일 / Per-ROM-set configuration files

설계: [docs/design/20260820-497-romset-config-files.md](../design/20260820-497-romset-config-files.md)

## 한국어

### 1. 파일 위치와 이름

rePIU는 실행한 롬셋과 같은 이름의 INI 파일을 `cfg` 디렉터리에서 읽는다.

```
cfg/pumpit1.ini
cfg/pumpit3.ini
cfg/pumpitp3.ini
```

디렉터리는 다음 순서로 찾고, 처음 존재하는 것 하나만 쓴다.

1. 환경 변수 `REPIU_CFG_DIR`
2. 현재 작업 디렉터리의 `cfg`
3. 실행 파일이 있는 디렉터리의 `cfg`

`roms`와 `build/runtime_mounts`처럼 기본은 현재 작업 디렉터리 기준이다.

### 2. 첫 실행 시 자동 생성

설정 파일이 없으면 rePIU가 만든다. 만들어진 파일의 입력 항목은 **전부 주석 처리**되어
있고, 주석의 값은 지금 실제로 적용 중인 설정이다.

```ini
[Input]
; Player 1 stage panels (I/O port 0x02A8)
;P1_UP_LEFT    = Q
;P1_UP_RIGHT   = E
```

바꾸고 싶은 줄만 주석(`;`)을 지우면 된다. 나머지는 주석인 채로 두면 기본값이 그대로
쓰인다.

* **기존 파일은 절대 덮어쓰지 않는다.** 고친 내용이 다음 실행에서 되돌아가지 않는다.
* 쓸 수 없는 위치(읽기 전용 등)면 경고만 남기고 그대로 실행한다.
* `REPIU_CFG_WRITE_DEFAULT=0`으로 자동 생성을 끌 수 있다.

### 3. 여러 롬셋에 한 번에 적용하기

롬셋 파일은 **부모 롬셋 파일** 위에 얹힌다. 대부분의 롬셋이 `pumpitup`을 부모로 두므로,
공용 설정은 `cfg/pumpitup.ini`에 넣으면 된다.

```
내장 기본값  →  cfg/pumpitup.ini  →  cfg/pumpit3.ini  →  cfg/pumpit3a.ini
```

덮어쓰기는 **키 단위**다. 자식 파일에 없는 항목은 부모 값이 그대로 남는다. 예를 들어
`cfg/pumpitup.ini`에 1P 발판을 적어 두고 `cfg/pumpit3.ini`에는 `TEST`만 적으면,
pumpit3는 공용 1P 배치에 자기만의 TEST 키를 쓴다.

`cfg/pumpitup.ini`는 자동으로 만들어지지 않는다. 직접 만들어야 한다.

### 4. 문법

```ini
; 주석
# 이것도 주석

[Input]
NAME = key            호스트 키 하나
NAME = key1, key2     여러 개. 어느 쪽을 눌러도 동작
NAME = Ctrl+F1        조합키. 기본 키가 마지막
NAME =                값을 비우면 이 입력을 끈다
```

* 섹션 이름과 키 이름은 대소문자와 언더스코어를 무시한다. `Keypad7`, `KEYPAD_7`,
  `keypad7`이 모두 같다.
* 값의 따옴표는 벗겨진다. `'Q'`와 `Q`는 같다.
* 같은 키가 여러 번 나오면 마지막 것이 이긴다.
* 모르는 섹션, 키, 키 이름은 경고만 남기고 무시한다. **설정 파일의 오타로 게임이 실행되지
  않는 일은 없다.**

### 4.1 입력 끄기

값을 비우면 그 입력이 꺼진다. 어떤 키로도 동작하지 않는다.

```ini
[Input]
TEST    =
SERVICE =
```

이렇게 적으면 F1과 F2를 눌러도 게임은 TEST와 SERVICE가 눌린 것을 보지 못한다. **주석을 반드시
지워야 한다.** `;TEST =` 처럼 앞에 `;`가 남아 있으면 그 줄은 주석이라 아무 효과가 없고 기본값
`F1`이 그대로 동작한다. 자동 생성된 파일은 모든 줄이 주석 상태로 시작한다.

항목은 그 입력의 키 목록을 **통째로 교체**한다. 부모 파일 값에 더하는 것이 아니라, 적은 값이
그 입력에 대한 전부다. 그래서 빈 값은 "이 입력에 키가 없다"는 완전한 지정이 된다.

키 이름을 잘못 적어도 (`TEST = NoSuchKey`) 결과는 같지만 경고가 남는다. 일부만 잘못됐으면
(`TEST = F9, NoSuchKey`) 유효한 쪽만 적용된다.

### 5. 입력 이름

| 키 이름 | 설명 | 기본값 |
|---|---|---|
| `P1_UP_LEFT` | 1P 좌상단 발판 | `Q` |
| `P1_UP_RIGHT` | 1P 우상단 발판 | `E` |
| `P1_CENTER` | 1P 가운데 발판 | `S` |
| `P1_DOWN_LEFT` | 1P 좌하단 발판 | `Z` |
| `P1_DOWN_RIGHT` | 1P 우하단 발판 | `C` |
| `P2_UP_LEFT` | 2P 좌상단 발판 | `Keypad7, Home` |
| `P2_UP_RIGHT` | 2P 우상단 발판 | `Keypad9, PageUp` |
| `P2_CENTER` | 2P 가운데 발판 | `Keypad5, Clear` |
| `P2_DOWN_LEFT` | 2P 좌하단 발판 | `Keypad1, End` |
| `P2_DOWN_RIGHT` | 2P 우하단 발판 | `Keypad3, PageDown` |
| `TEST` | 테스트 버튼 | `F1` |
| `SERVICE` | 서비스 버튼 | `F2` |
| `CLEAR` | CLEAR 버튼 | `F3` |
| `COIN1` | 코인 투입 | `F5` |

2P 기본값에 숫자패드와 편집키가 둘 다 있는 이유는 NumLock 상태 때문이다. NumLock이 꺼져
있으면 Windows가 숫자패드 7을 `Home`으로 보고한다. 둘 다 걸어 두면 NumLock 상태와 무관하게
동작한다.

`COIN2`와 캐비닛 보조 입력은 아직 지원하지 않는다. 해당 I/O 포트 비트가 확정되지 않았다.

### 6. 설정할 수 있는 키 이름

| 분류 | 이름 |
|---|---|
| 문자 | `A` ~ `Z` |
| 숫자 | `0` ~ `9` |
| 기능키 | `F1` ~ `F12` |
| 숫자패드 | `Keypad0` ~ `Keypad9`, `KeypadEnter`, `KeypadPlus`, `KeypadMinus`, `KeypadMultiply`, `KeypadDivide`, `KeypadPeriod` |
| 이동·편집 | `Up`, `Down`, `Left`, `Right`, `Home`, `End`, `PageUp`, `PageDown`, `Insert`, `Delete`, `Clear` |
| 기타 | `Space`, `Enter`, `Tab`, `Backspace`, `Escape` |
| 수식키 | `LeftShift`, `RightShift`, `LeftCtrl`, `RightCtrl`, `LeftAlt`, `RightAlt` |

같은 목록이 생성된 설정 파일의 주석에도 들어 있다.

`Clear`는 NumLock이 꺼진 숫자패드 5의 이름이다. 시스템 입력 `CLEAR`와는 관계가 없다.

Windows 키는 지원하지 않는다. 대부분의 조합을 운영체제가 먼저 가로채므로 설정해도 실제로
게임에 도달하지 않는다.

### 7. 조합키

수식키를 `+`로 잇고 기본 키를 마지막에 쓴다.

```ini
[Input]
TEST     = Ctrl+F1
SERVICE  = Ctrl+Shift+F2
CLEAR    = LeftAlt+F3, F3
```

* `Ctrl`, `Shift`, `Alt`는 좌우 어느 쪽이든 동작한다. 한쪽만 원하면 `LeftCtrl`처럼 쓴다.
* 수식키를 붙인 설정은 **정확히 일치**해야 한다. `Ctrl+F1`은 Ctrl+Shift+F1에서 동작하지
  않는다.
* 수식키가 없는 설정은 수식키를 따지지 않는다. `P1_UP_LEFT = Q`면 Ctrl을 누른 채 Q를 눌러도
  발판이 눌린다.
* 예외는 **같은 키를 두고 겹칠 때**다. `TEST = Ctrl+F1`과 `CLEAR = F1`이 함께 있으면
  F1만 눌렀을 때는 CLEAR, Ctrl+F1은 TEST가 된다.
* NumLock, CapsLock, ScrollLock 상태는 어떤 설정에도 영향을 주지 않는다.

조합키는 TEST, SERVICE 같은 캐비닛 버튼용이다. **발판 입력에는 권장하지 않는다.** 창이 키를
받는 경로에서는 수식키 판정이 기본 키를 누른 순간의 상태로 고정되므로, 기본 키를 누른 뒤
수식키에서 손을 떼도 발판이 눌린 상태로 남는다.

### 8. 문제 해결

**설정이 반영되지 않는다**

로더 시작 로그를 확인한다. 어느 디렉터리를 쓰고 어떤 파일을 적용했는지 남는다.

```
[loader] Config directory: cfg
[loader] Config applied: cfg/pumpit1.ini
[loader] Config created: cfg/pumpit1.ini
[loader] Config: ...
```

* `Config files applied: none (built-in defaults)`이면 파일을 못 찾은 것이다. 실행할 때의
  작업 디렉터리와 `cfg` 위치를 확인한다.
* `Config: ...` 경고가 있으면 그 줄에 문제가 있다는 뜻이다. 줄 번호가 함께 나온다.
* 항목의 주석(`;`)을 지웠는지 확인한다. 자동 생성된 파일은 전부 주석 상태다.

**키를 눌러도 게임이 반응하지 않는다**

`[repiu-input]` 로그가 press/release를 한 줄씩 남긴다. 이 로그가 나오는데 게임이 반응하지
않으면 입력은 게스트까지 도달한 것이고, 로그가 없으면 매핑 문제다.

**2P 발판이 안 먹는다**

NumLock 상태를 확인한다. 기본값은 숫자패드와 편집키를 둘 다 걸어 두므로 양쪽 다 동작해야
정상이다. 한쪽만 남기도록 고쳤다면 NumLock 상태에 따라 안 먹을 수 있다.

---

## English

### 1. File location and name

rePIU reads an INI file named after the launched ROM set from the `cfg` directory.

```
cfg/pumpit1.ini
cfg/pumpit3.ini
cfg/pumpitp3.ini
```

The directory is located in this order, using the first one that exists:

1. The `REPIU_CFG_DIR` environment variable
2. `cfg` under the current working directory
3. `cfg` next to the executable

The default is relative to the working directory, matching `roms` and
`build/runtime_mounts`.

### 2. Generated on first run

If the config file does not exist, rePIU creates it. Every input entry in the created
file is **commented out**, and the value in the comment is the setting currently in
effect.

```ini
[Input]
; Player 1 stage panels (I/O port 0x02A8)
;P1_UP_LEFT    = Q
;P1_UP_RIGHT   = E
```

Remove the `;` from just the lines you want to change. Anything left commented keeps
using the default.

* **An existing file is never overwritten.** Your edits do not revert on the next run.
* If the location cannot be written (read-only, for example), rePIU warns and keeps
  running.
* `REPIU_CFG_WRITE_DEFAULT=0` turns generation off.

### 3. Applying a setting to several ROM sets

A ROM set's file layers on top of its **parent ROM set's** file. Most sets name
`pumpitup` as their parent, so shared settings belong in `cfg/pumpitup.ini`.

```
built-in defaults  ->  cfg/pumpitup.ini  ->  cfg/pumpit3.ini  ->  cfg/pumpit3a.ini
```

Overriding is **per key**: an entry absent from the child file keeps the parent's value.
Put the P1 panels in `cfg/pumpitup.ini` and only `TEST` in `cfg/pumpit3.ini`, and
pumpit3 uses the shared P1 layout with its own TEST key.

`cfg/pumpitup.ini` is not generated automatically; create it yourself.

### 4. Syntax

```ini
; comment
# also a comment

[Input]
NAME = key            one host key
NAME = key1, key2     several; any of them works
NAME = Ctrl+F1        modifier combination, base key last
NAME =                no value: turn this input off
```

* Section and key names ignore case and underscores, so `Keypad7`, `KEYPAD_7`, and
  `keypad7` are the same.
* Quotes around a value are stripped: `'Q'` and `Q` are the same.
* When a key appears more than once, the last one wins.
* Unknown sections, keys, and key names produce a warning and are skipped. **A typo in a
  config file never stops the game from running.**

### 4.1 Turning an input off

Leaving a value empty turns that input off; no key drives it any more.

```ini
[Input]
TEST    =
SERVICE =
```

With this, pressing F1 or F2 no longer makes the game see TEST or SERVICE pressed. **The
`;` must be gone.** A line still written as `;TEST =` is a comment, has no effect at all,
and leaves the `F1` default working. Generated files start with every line commented out.

An entry **replaces** that input's key list rather than adding to the parent file's, so
what you write is the whole answer for that input — and an empty value is the complete
instruction "no key drives this".

A mistyped key name (`TEST = NoSuchKey`) reaches the same result but leaves a warning. A
partly invalid value (`TEST = F9, NoSuchKey`) applies its valid half.

### 5. Input names

| Key name | Meaning | Default |
|---|---|---|
| `P1_UP_LEFT` | P1 upper-left panel | `Q` |
| `P1_UP_RIGHT` | P1 upper-right panel | `E` |
| `P1_CENTER` | P1 center panel | `S` |
| `P1_DOWN_LEFT` | P1 lower-left panel | `Z` |
| `P1_DOWN_RIGHT` | P1 lower-right panel | `C` |
| `P2_UP_LEFT` | P2 upper-left panel | `Keypad7, Home` |
| `P2_UP_RIGHT` | P2 upper-right panel | `Keypad9, PageUp` |
| `P2_CENTER` | P2 center panel | `Keypad5, Clear` |
| `P2_DOWN_LEFT` | P2 lower-left panel | `Keypad1, End` |
| `P2_DOWN_RIGHT` | P2 lower-right panel | `Keypad3, PageDown` |
| `TEST` | Test button | `F1` |
| `SERVICE` | Service button | `F2` |
| `CLEAR` | Clear button | `F3` |
| `COIN1` | Coin insert | `F5` |

The P2 defaults list both a keypad key and an editing key because of NumLock: with
NumLock off, Windows reports keypad 7 as `Home`. Binding both makes the panel work
either way.

`COIN2` and the auxiliary cabinet inputs are not supported yet; their I/O port bits are
not identified.

### 6. Configurable key names

| Group | Names |
|---|---|
| Letters | `A` – `Z` |
| Digits | `0` – `9` |
| Function | `F1` – `F12` |
| Keypad | `Keypad0` – `Keypad9`, `KeypadEnter`, `KeypadPlus`, `KeypadMinus`, `KeypadMultiply`, `KeypadDivide`, `KeypadPeriod` |
| Navigation and editing | `Up`, `Down`, `Left`, `Right`, `Home`, `End`, `PageUp`, `PageDown`, `Insert`, `Delete`, `Clear` |
| Other | `Space`, `Enter`, `Tab`, `Backspace`, `Escape` |
| Modifiers | `LeftShift`, `RightShift`, `LeftCtrl`, `RightCtrl`, `LeftAlt`, `RightAlt` |

The same list appears in the comments of a generated config file.

`Clear` is the name of keypad 5 with NumLock off. It is unrelated to the `CLEAR` system
input.

The Windows key is not supported: the operating system intercepts most of its
combinations, so a binding would never reach the game.

### 7. Key combinations

Join modifiers with `+` and put the base key last.

```ini
[Input]
TEST     = Ctrl+F1
SERVICE  = Ctrl+Shift+F2
CLEAR    = LeftAlt+F3, F3
```

* `Ctrl`, `Shift`, and `Alt` accept either side of the keyboard. Write `LeftCtrl` to
  require one specific side.
* A binding with modifiers must match **exactly**: `Ctrl+F1` does not fire on
  Ctrl+Shift+F1.
* A binding without modifiers ignores modifier state. With `P1_UP_LEFT = Q`, holding
  Ctrl and pressing Q still presses the panel.
* The exception is **contention over the same key**. With `TEST = Ctrl+F1` and
  `CLEAR = F1`, plain F1 means CLEAR while Ctrl+F1 means TEST.
* NumLock, CapsLock, and ScrollLock never affect any binding.

Combinations are meant for cabinet buttons such as TEST and SERVICE. **They are not
recommended for stage panels**: on the window input path the modifier test is fixed at
the instant the base key goes down, so releasing the modifier afterwards leaves the panel
held until the base key is released.

### 8. Troubleshooting

**A setting has no effect**

Check the loader's startup log. It records which directory was used and which files were
applied.

```
[loader] Config directory: cfg
[loader] Config applied: cfg/pumpit1.ini
[loader] Config created: cfg/pumpit1.ini
[loader] Config: ...
```

* `Config files applied: none (built-in defaults)` means no file was found. Check the
  working directory you launched from and where `cfg` actually is.
* A `Config: ...` warning names the problem and the line number.
* Check that you removed the leading `;`. Generated files are fully commented out.

**A key does nothing in the game**

The `[repiu-input]` log prints one line per press and per release. If those lines appear
but the game does not react, the input reached the guest and the mapping is fine; if they
do not appear, the mapping is the problem.

**The P2 panels do not respond**

Check NumLock. The defaults bind both the keypad key and the editing key, so both should
work. If you narrowed a binding to one of them, NumLock state decides whether it arrives.
