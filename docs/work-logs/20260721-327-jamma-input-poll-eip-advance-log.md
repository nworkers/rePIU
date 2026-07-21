# 20260721-327-jamma-input-poll-eip-advance-log

## 작업 결과 요약 (Summary)

JAMMA 입력 포트(`0x02A8` P1, `0x02A9` System, `0x02AA` P2) 읽기가 최초 1회만
반영되고 이후 무력화되던 문제를 수정했습니다. 근인은 입력 IN 명령을
`apply_nop_patch()`로 덮어써 게임의 매 프레임 폴링을 파괴한 것이었고, EEPROM
읽기와 동일하게 `win32_context->Eip += instruction_len;`으로 교체했습니다.

Fixed JAMMA input reads that only reflected the first poll and were then
disabled. Root cause was `apply_nop_patch()` overwriting the guest IN
instruction, destroying the game's per-frame polling. Replaced it with
`win32_context->Eip += instruction_len;`, matching the EEPROM read path.

## 분석 근거 (Analysis)

* `WriteGuestBytes` → `NoteSuccessfulAotGuestWrite`는 guest 바이트 수정 시 해당
  AOT 페이지를 retire/재변환시키므로 NOP 패치가 실제로 적용됨 → Task 326의 60초
  스모크 테스트는 hang 없이 통과했으나 입력은 첫 프레임 이후 죽어 있었음.
* 증상 1(무입력이 눌림으로 오인): 첫 폴링 후 EAX가 active-low idle `0xFF`로
  갱신되지 않아 스테일/latch 값이 입력으로 오인됨.
* 증상 2(press/release 미반영): 최초 1프레임 외 모든 입력 전이 무시됨.

## 변경 파일 (Changes)

* `src/platform/win32/io/port_io_emulator.cpp`
  - JAMMA 입력 branch: `apply_nop_patch();` → `win32_context->Eip += instruction_len;` (+ 근거 주석)
* `docs/analysis/piu-io-port-specification.md`
  - 입력 포트도 NOP 패치 없이 EIP 전진으로 재평가함을 명시 (한/영).
* `docs/design/20260721-327-jamma-input-poll-eip-advance.md` (신규)
* `docs/work-orders/20260721-327-jamma-input-poll-eip-advance.md` (신규)

## 검증 (Verification)

* **전체 빌드 불가**: 현재 Linux 컨테이너에 MSVC/Win32 SDK가 없어
  `build_win32_x86.ps1`(MSVC + WIN32 + 32-bit) 빌드를 수행할 수 없습니다.
  (AGENTS.md "빌드 검증이 불가능하면 이유를 작업 로그에 남긴다"에 따라 기록.)
* **정적 검토**: 변경은 동일 함수 스코프에 이미 존재하는 `win32_context`,
  `instruction_len`만 사용하는 한 줄 교체이며, EEPROM 읽기 경로(line 216)와
  동일 패턴이라 컴파일·회귀 리스크 없음. `apply_nop_patch` 람다는 쓰기/초기화/
  deferred 경로에서 계속 사용되어 미사용 경고 없음.
* **권장 실기 검증(Windows)**: `build_win32_x86.ps1` 재빌드 후 `pumpit1` 구동,
  발판 키 press/release가 프레임 단위로 반영되고 무입력 시 idle로 복귀하는지 확인.
