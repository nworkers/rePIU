# 작업 로그: 게임과 런처의 게임패드·조이스틱 입력 (issue #34)

설계: `docs/design/20261009-i034-gamepad-input.md`
작업 지시: `docs/work-orders/20261009-i034-gamepad-input.md`

## 한 일

* **이름과 상태(`repiu::input`)**: `host_pad_binding`이 `Pad<N>_<버튼>`, `Pad<N>_Left/RightTrigger`,
  `Joy<N>_Button<K>`, `Joy<N>_Hat<H><방향>`을 판별·파싱·포맷한다(대소문자·밑줄 무시, `South` 등 위치
  이름도 받음). `host_pad_state`는 장치 번호별 버튼·트리거·hat 상태와 번호 배정 표
  (`HostPadSlotTable`, 연결 순서·가장 작은 빈 번호·분리될 때까지 유지)를 둔다.
* **바인딩**: `JammaInputBinding`에 패드 별칭 4개를 따로 두고, 값의 각 항목을 모양으로 갈라 키나
  패드로 파싱한다. 키·패드 각각 4개를 넘으면 경고한다. 기본값 문자열에 Pad1(1P)·Pad2(2P)를 붙였다.
  `FormatJammaBinding`은 키 다음에 패드를 쓰고, `ComputeJammaPadMask`가 상태에서 입력 마스크를 낸다.
* **엔진**: `SdlPadInput`이 host 스레드의 첫 이벤트 펌프에서 `SDL_INIT_GAMEPAD`를 켜고 이미 연결된
  장치를 연 뒤, 추가·제거·버튼·트리거·hat 이벤트로 상태를 갱신한다. 마스크가 바뀌면
  `PublishJammaPadMask`로 게시한다. Glide 백엔드의 이벤트 루프는 바뀐 입력만 타임라인 edge로 기록하고,
  키·패드가 같은 입력을 누르고 있을 때 한쪽 release를 미룬다. 포커스를 잃으면 패드도 모두 뗀다.
  창을 닫을 때 패드를 닫는다. `ScanJammaPort8`(폴링)와 `CaptureCurrentJammaPressedMask`는 게시된 마스크를
  OR한다(키보드만 보는 `CaptureKeyboardJammaPressedMask`를 따로 둠).
* **런처**: `SDL_INIT_GAMEPAD`를 켜 ImGui SDL3 백엔드의 게임패드 내비게이션이 동작하게 했다.
* **설정 파일 생성**: 주석 블록에 패드 이름 절을 더했다(버튼 목록은 이름 표에서 생성, 78열 안).
* **probe**: `host_pad_input`(core·aot, 장치 없이 8개 항목), 설정 probe의 기본값 단언 하나 갱신.
* **문서**: 사용자 가이드 §5 기본값 표와 §6.1, ARCHITECTURE 새 절, README, 497 설계 §17.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Release·Linux x64 Release 빌드 | 오류 0 |
| `repiu_aot_probe --host-pad-input`(Win32) | shape·parse·rejects·format·mixed_value·defaults·mask·slots 모두 true |
| `repiu_aot_probe --romset-config`(Win32) | 94개 검사, 실패 0(생성 파일 왕복 포함) |
| Win32 aot_probe 전체 체인 | exit 0, 555줄 |
| core probe | Win32·Linux x64 모두 실패 0 |

## 확인하지 않은 것

* **실제 장치.** 이 환경에 연결된 게임패드나 발판이 없어 장치 이벤트 경로(`SdlPadInput`과 백엔드
  연결)는 실행으로 확인하지 못했다. 장치 없이 동작이 그대로인지 보는 게임 실행도 하지 않았다
  (사용자 확인 후 진행).
* Linux i386 빌드.

---

# Work log: gamepad and joystick input for the game and the launcher (issue #34)

**Done.** In `repiu::input`, `host_pad_binding` detects, parses and formats `Pad<N>_<Button>`,
`Pad<N>_Left/RightTrigger`, `Joy<N>_Button<K>` and `Joy<N>_Hat<H><Dir>` (ignoring case and
underscores, positional names such as `South` accepted), and `host_pad_state` holds per-device
buttons, triggers and hats with a numbering table (connection order, lowest free number, kept until
removal). `JammaInputBinding` keeps four pad aliases beside its keys; each value item is routed by
its shape, each list warns past four, the default strings gain Pad1 (P1) and Pad2 (P2),
`FormatJammaBinding` writes pads after keys, and `ComputeJammaPadMask` maps state to inputs. In the
engine, `SdlPadInput` initializes `SDL_INIT_GAMEPAD` on the host thread's first event pump, opens
attached devices, updates state from add, remove, button, trigger and hat events, and publishes the
mask on change; the Glide backend's event loop records only the changed inputs as timeline edges,
defers a release while the other source still holds the input, releases pads on focus loss and
closes them with the window; `ScanJammaPort8` and `CaptureCurrentJammaPressedMask` OR in the
published mask (with a keyboard-only `CaptureKeyboardJammaPressedMask`). The launcher initializes
`SDL_INIT_GAMEPAD` so ImGui's SDL3 gamepad navigation works. The generated config comment block
lists the pad names (the button list comes from the name table, within 78 columns). Probes: a new
`host_pad_input` (core and aot, eight checks without a device) and one updated default assertion
in the config probe. Docs: the user guide's §5 defaults and §6.1, a new ARCHITECTURE section,
README, and the 497 design's §17.

**Verification.** Win32 and Linux x64 Release builds with no errors; `--host-pad-input` passes all
eight checks; `--romset-config` passes 94 checks including the generated-file round trip; the full
Win32 aot_probe chain exits 0 with 555 lines; the core probe has no failures on Win32 or Linux x64.

**Not checked.** Real devices: no gamepad or dance pad is attached here, so the device event path
(`SdlPadInput` and the backend wiring) has not been exercised, and no game run checked that nothing
changes without a device (pending the user's go-ahead). The Linux i386 build.
