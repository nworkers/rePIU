# 작업 로그: 게임패드 종료 조합 LT+RT+L3+R3 1초 (issue #52)

설계: `docs/design/20261010-i052-pad-exit-chord.md`
작업 지시: `docs/work-orders/20261010-i052-pad-exit-chord.md`

## 한 일

* **입력 계층**: `repiu/input/pad_exit_chord`의 `IsPadExitChordDown`(한 패드에서 두 트리거와 두 스틱
  클릭이 모두 눌림)과 `PadExitChordTimer`(1초 연속에 한 번 발동, 떼면 다시 준비). 트리거 기준값을
  `kHostPadTriggerThreshold`(16384)로 `host_pad_state.h`에 옮기고 `SdlPadInput`이 그것을 쓴다.
* **게임**: `SdlPadInput::state()`를 내보이고, 백엔드가 이벤트 펌프 끝마다 타이머를 갱신한다. 발동하면
  `[repiu-pad] exit chord held for 1 s: exit requested`와 함께 창 닫기와 같은 `exit_requested_`.
* **런처**: 프레임마다 `SDL_GetGamepads`로 연결된 패드를 읽어(ImGui가 열지 않은 것은 열어 두고 끝에 닫음)
  같은 판정을 한다. 발동하면 창을 닫고 `closed_by_exit_chord`를 남기며, 로더는
  `Launcher closed without starting a ROM set (gamepad exit chord held for 1 s)`를 기록한다. 안내 문구에
  조합을 더했다.
* **probe**: `host_pad_input_exit_chord`(한 패드 넷 / 하나 빠짐 / 두 패드에 나눔 / 기준값, 1초 직전·직후,
  한 번만, 떼고 다시, 중간에 끊김).
* **문서**: 설계·작업 지시, 설정 가이드 패드 절, 런처 업데이트 가이드의 스팀덱 절, README, ARCHITECTURE.

## 검증

| 검증 | 결과 |
|---|---|
| Linux x64·i386 Release 빌드 | 통과, 새 경고 없음 |
| core probe | 두 아키텍처 모두 `host_pad_input_exit_chord=true`, 실패 0 |
| 실제 패드(Xbox Series X, Linux x64, 런처 → pumpitea) | 게임 중 조합 1초: `exit chord held for 1 s: exit requested`, `reason=exit-requested`로 정상 종료, 런처로 복귀. 조합 중 `CLEAR PRESSED`가 게임에 들어감(결정대로). 이어서 런처가 닫힘. 사용자가 세 단계(게임 중 종료, 짧게 누르기, 런처 종료)를 화면으로 확인(2026-10-10) |

## 확인하지 않은 것

* 짧게 누르기와 런처 종료는 로그로 구분되지 않았다(당시 런처는 닫힌 이유를 남기지 않았다). 이후 런처 로그에
  이유를 더했고, 사용자의 화면 확인에 기댄다.
* Win32 빌드(CI 확인 예정)와 스팀덱 실기.

---

# Work log: the gamepad exit chord, LT+RT+L3+R3 for one second (issue #52)

**Done.** In the input layer, `repiu/input/pad_exit_chord`'s `IsPadExitChordDown` (both triggers and both
stick clicks held on one pad) and `PadExitChordTimer` (fires once after a second of unbroken holding,
re-armed by release); the trigger threshold moved to `host_pad_state.h` as `kHostPadTriggerThreshold` (16384)
and `SdlPadInput` uses it. The game: `SdlPadInput::state()` is exposed and the backend updates the timer at the
end of every event pump, logging `[repiu-pad] exit chord held for 1 s: exit requested` and setting the same
`exit_requested_` closing the window does. The launcher reads the connected pads every frame through
`SDL_GetGamepads` (opening those ImGui has not, closing them at the end), applies the same check, closes and
reports `closed_by_exit_chord`, which the loader logs as `Launcher closed without starting a ROM set (gamepad
exit chord held for 1 s)`; its hint line names the chord. The `host_pad_input_exit_chord` probe covers four on
one pad, one missing, split across two pads, the threshold, just before and after a second, once only,
release and hold again, and a break midway. Docs: design, work order, the settings guide's pad section, the
launcher update guide's Steam Deck section, README, ARCHITECTURE.

**Verified.** Linux x64 and i386 Release builds with no new warnings; the core probe reports
`host_pad_input_exit_chord=true` and no failures on both. On a real pad (Xbox Series X, Linux x64, launcher →
pumpitea): holding the chord in the game logged `exit chord held for 1 s: exit requested`, shut down normally
with `reason=exit-requested` and returned to the launcher; the game saw `CLEAR PRESSED` during the chord, as
decided; the launcher then closed. The user confirmed all three steps (quit in a game, a short press, quit in
the launcher) on screen (2026-10-10).

**Not checked.** The short press and the launcher's quit could not be told apart in that log (the launcher did
not yet record why it closed); the reason is logged now, and those two rest on the user's on-screen check. The
Win32 build (CI to check) and a real Steam Deck.
