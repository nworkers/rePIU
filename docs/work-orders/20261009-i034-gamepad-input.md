# 작업 지시: 게임과 런처의 게임패드·조이스틱 입력 (issue #34)

설계: `docs/design/20261009-i034-gamepad-input.md`

1. `repiu/input/host_pad_binding.h/.cpp`: `Pad<N>_`·`Joy<N>_` 이름 판별, 파싱, 포맷, 버튼 이름 표.
2. `repiu/input/host_pad_state.h/.cpp`: 장치 번호별 상태, 별칭 판정, 번호 배정 표.
3. `jamma_input_bindings`: 패드 별칭 4개, 키·패드가 섞인 값 파싱, 기본값(Pad1 → 1P, Pad2 → 2P),
   포맷, `ComputeJammaPadMask`.
4. 엔진: `PublishJammaPadMask`/`PublishedJammaPadMask`, `SdlPadInput`(host 스레드), Glide 백엔드의
   이벤트 루프 연결(edge 기록, 키·패드 겹침, 포커스), `ScanJammaPort8`와
   `CaptureCurrentJammaPressedMask`의 OR.
5. 런처: `SDL_INIT_GAMEPAD`.
6. 설정 파일 생성: 패드 이름 주석 블록.
7. probe `host_pad_input`(core·aot), 설정 probe의 기본값 단언 갱신. CMake.
8. 문서: 사용자 가이드 §5·§6.1, ARCHITECTURE, README, 497 설계 §17 표시.
9. 검증: Win32·Linux x64 빌드, probe, 장치 없이 기존 동작 유지. 실제 장치는 사용자 환경.

완료 기준: 장치가 없을 때 동작이 변경 전과 같고, 표준 게임패드와 번호로 건 조이스틱이 설정대로
JAMMA 입력을 누른다.

---

# Work order: gamepad and joystick input for the game and the launcher (issue #34)

(1) Pad name detection, parsing, formatting and the button table. (2) Per-device state, alias
tests and the numbering table. (3) Pad aliases in the JAMMA bindings, mixed values, defaults (Pad1
for P1, Pad2 for P2), formatting and `ComputeJammaPadMask`. (4) The published mask, `SdlPadInput`
on the host thread, the backend event loop (edges, key and pad overlap, focus), and the OR in
`ScanJammaPort8` and `CaptureCurrentJammaPressedMask`. (5) `SDL_INIT_GAMEPAD` in the launcher. (6)
Pad names in the generated config comment block. (7) The `host_pad_input` probe (core and aot), the
config probe's default assertion, CMake. (8) The user guide (§5, §6.1), ARCHITECTURE, README, and
the 497 design's §17. (9) Verify: Win32 and Linux x64 builds, probes, unchanged behavior with no
device; real devices on the user's setup. Done when nothing changes without a device and a standard
gamepad and a joystick bound by number press JAMMA inputs as configured.
