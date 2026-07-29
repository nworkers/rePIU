# 2P NumLock OFF 숫자패드 입력 작업 로그
# P2 NumLock-Off Numeric-Keypad Input Work Log

* 작업 번호 / Task: 362
* 작성일 / Date: 2026-07-30
* 브랜치 / Branch: `fix/jamma-system-key-mapping`

## 1. 결과
## 1. Result

P2 Center 입력을 NumLock ON의 `VK_NUMPAD5`에서 NumLock OFF 숫자패드 5의
`VK_CLEAR`로 변경했다. 기존 방향 입력과 결합하면 2P는 NumLock OFF 상태에서
숫자패드 7, 9, 5, 1, 3을 사용한다.

Changed P2 Center from the NumLock-on `VK_NUMPAD5` to `VK_CLEAR`, emitted by
numeric-keypad 5 while NumLock is off. Combined with the existing directional
inputs, P2 now uses keypad 7, 9, 5, 1, and 3 with NumLock off.

## 2. 변경
## 2. Changes

* `src/platform/win32/io/port_io_emulator.cpp`
  * `0x02AA`의 P2 Center 키를 `VK_CLEAR`로 변경했다.
* `docs/analysis/piu-io-port-specification.md`
  * NumLock OFF 2P 숫자패드 배치와 Win32 가상키를 기록했다.
* Task 362 설계 및 작업 지시 문서를 추가했다.

* `src/platform/win32/io/port_io_emulator.cpp`
  * Changed the P2 Center key on `0x02AA` to `VK_CLEAR`.
* `docs/analysis/piu-io-port-specification.md`
  * Recorded the NumLock-off P2 keypad layout and Win32 virtual keys.
* Added the Task 362 design and work-order documents.

## 3. 검증
## 3. Verification

### 빌드 / Build

다음 Win32 x86 Debug 빌드가 오류 없이 성공했다.

The following Win32 x86 Debug build completed without errors.

```text
cmake --build build\win32_x86_debug --config Debug \
  --target repiu_loader_win32 repiu_supervisor_win32
```

### 실제 입력 / Live input

`aot-dbt`로 `pumpit1`을 실행하고 `0x02AA` 폴링 시작 후 NumLock을 끈 상태에서
숫자패드 5 스캔 코드 `0x4C`를 주입했다.

Ran `pumpit1` with `aot-dbt`, waited for `0x02AA` polling, disabled NumLock,
and injected numeric-keypad 5 scan code `0x4C`.

```text
[repiu-input] P2-Center PRESSED  port=0x02AA value=0xFB
[repiu-input] P2-Center released port=0x02AA value=0xFF
```

검증 로그:
`build/task362_input_20260730_024601/stderr.log`

Verification log:
`build/task362_input_20260730_024601/stderr.log`

검증 전 NumLock 상태는 ON이었으며, 검증 종료 후 원래 ON 상태로 복원됨을
확인했다.

NumLock was on before verification and was confirmed restored to its original
on state afterward.

## 4. 회귀 범위
## 4. Regression Scope

P2의 `VK_HOME`, `VK_PRIOR`, `VK_END`, `VK_NEXT` 방향 입력과 시스템 포트
F3 CLEAR 입력은 변경하지 않았다.

The P2 `VK_HOME`, `VK_PRIOR`, `VK_END`, and `VK_NEXT` direction inputs and the
system-port F3 CLEAR input were not changed.
