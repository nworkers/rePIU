# 작업 지시: 게임패드 종료 조합 LT+RT+L3+R3 1초 (issue #52)

설계: `docs/design/20261010-i052-pad-exit-chord.md`

1. `repiu/input/pad_exit_chord.h/.cpp`: `IsPadExitChordDown`, `PadExitChordTimer`. 트리거 기준값을
   `kHostPadTriggerThreshold`로 입력 계층에 두고 `SdlPadInput`이 그것을 쓴다.
2. 엔진: `SdlPadInput`이 상태를 내보이고, 백엔드가 이벤트 펌프마다 타이머를 갱신해 발동 시 로그와
   `exit_requested_`.
3. 런처: 프레임마다 연결된 게임패드로 `HostPadState`를 채워 타이머 갱신, 발동 시 창 닫기.
4. `host_pad_input` probe에 조합·타이머 항목. CMake.
5. 문서: 설정 가이드 패드 절, 런처 업데이트 가이드의 스팀덱 절, README, ARCHITECTURE, 작업 로그.
6. 검증: Linux x64·i386 빌드와 core probe, 실제 패드(게임·런처).

완료 기준: 한 패드로 넷을 1초 누르면 게임은 런처로, 런처는 종료하며, 1초 전에 떼면 아무 일도 없다.

---

# Work order: the gamepad exit chord, LT+RT+L3+R3 for one second (issue #52)

(1) `repiu/input/pad_exit_chord`: `IsPadExitChordDown` and `PadExitChordTimer`; the trigger threshold moves to
the input layer as `kHostPadTriggerThreshold` and `SdlPadInput` uses it. (2) Engine: `SdlPadInput` exposes its
state and the backend updates the timer every event pump, logging and setting `exit_requested_` when it fires.
(3) Launcher: fill a `HostPadState` from the connected gamepads every frame, update the timer, close when it
fires. (4) The chord and timer checks in the `host_pad_input` probe; CMake. (5) Docs: the settings guide's pad
section, the launcher update guide's Steam Deck section, README, ARCHITECTURE, the work log. (6) Verify: Linux
x64 and i386 builds and the core probe, a real pad in a game and in the launcher. Done when holding the four on
one pad for a second returns a game to the launcher and quits the launcher, and releasing sooner does nothing.
