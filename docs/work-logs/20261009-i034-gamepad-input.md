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

## 기본값 변경 (2026-10-09, 사용자 확인 뒤)

사용자가 실제 패드로 동작을 확인한 뒤 기본 배치를 정했다. 처음 설계의 "D-pad 45° 회전 + Start 코인 +
스틱 클릭 TEST·SERVICE"를 아래로 바꿨다.

| 입력 | 1P(Pad1) / 2P(Pad2) |
|---|---|
| 좌상·우상 | `LeftShoulder` · `RightShoulder` |
| 좌하 | `DpadDown`, `DpadLeft` |
| 우하 | `B`, `DpadRight` |
| 가운데 | `A`, `Start` |
| `COIN1` | `Pad1_Back`, `Pad2_Back` |
| `TEST`, `SERVICE` | 없음(키보드만) |
| `CLEAR` | `Pad1_RightStick`(사용자가 언급하지 않아 유지) |

probe의 기본값·마스크 단언과 설정 probe의 기본값 단언을 새 배치로 고쳤고, 가이드·설계·ARCHITECTURE를
갱신했다. Win32 `--host-pad-input`·`--romset-config`(94개)·core probe, Linux x64 core probe 통과.
이어 가운데에 `Start`를 더했고(사용자 요청), 사용자가 실제 패드로 새 배치를 확인했다.

## 확인하지 않은 것

* **실제 장치.** 이 환경에는 장치가 없어 직접 실행하지 못했지만, 사용자가 실제 패드로 동작을
  확인했다(2026-10-09). 발판형 조이스틱(`Joy<N>_`)은 확인 여부를 따로 듣지 못했다.
* Linux i386 빌드.

## Linux 검증 (2026-10-10)

환경과 트리는 #24 작업 로그의 같은 절과 같다(Ubuntu 26.04.1, main `2809668`).

* **빌드**: Linux i386·x64 Release, 모든 기본 타깃 통과(런처 포함), 경고는 원래 있던 것뿐.
* **core probe**: 두 아키텍처 모두 `host_pad_input_all=true`, `launcher_all=true`,
  `core_probe_failures=0`.
* **여전히 확인하지 않은 것**: Linux에서의 실제 장치. 이 기기에는 게임패드·조이스틱이 연결되어
  있지 않다.

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

**Defaults changed (2026-10-09, after the user's check).** Having confirmed the feature on a real
pad, the user set the layout, replacing the first design's rotated D-pad, Start for coin and stick
clicks for TEST and SERVICE: the shoulder buttons for the upper panels, D-pad down and left for
down-left, `B` and D-pad right for down-right, `A` and `Start` for the center, either pad's `Back`
for `COIN1`,
no pad default for `TEST` and `SERVICE`, and `CLEAR` kept on Pad1's right stick click (not
mentioned). The probes' default and mask assertions and the config probe's default assertion follow
the new layout, and the guide, design and ARCHITECTURE are updated; the Win32 `--host-pad-input`,
`--romset-config` (94 checks) and core probes and the Linux x64 core probe pass. Start was then added
to the center at the user's request, and the user confirmed the new layout on a real pad.

**Not checked.** Nothing could be run on a device here, but the user confirmed the feature on a real
gamepad (2026-10-09); whether a dance-pad joystick (`Joy<N>_`) was tried was not reported. The Linux
i386 build.

**Linux verification (2026-10-10).** In the environment and tree of the same section in #24's work
log (Ubuntu 26.04.1, main `2809668`): Linux i386 and x64 Release builds of every default target,
the launcher included, pass with only pre-existing warnings, and the core probe reports
`host_pad_input_all=true`, `launcher_all=true` and `core_probe_failures=0` on both. Still not
checked: a real device on Linux; no gamepad or joystick is connected to this machine.
