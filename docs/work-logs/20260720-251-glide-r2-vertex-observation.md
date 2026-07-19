# Glide R2 GrVertex 관측 작업 로그

## 결과

`_GRDRAWTRIANGLE@12`의 최초 호출을 위한 비침습 관측을 추가했다. 세 정점 인자 포인터와 각 72바이트(18 dword)를 guest readable 검사 뒤 기록하며, 읽을 수 없는 포인터도 별도 flag로 남긴다. 실시간 로그에는 포인터와 readable 상태를, execution attempt에는 원시 dword를 남긴다.

기존 draw gate는 계속 stdcall ABI를 보존하는 no-op이다. 따라서 이 변경은 게임 렌더링 의미나 원본 제어 흐름을 바꾸지 않는다.

## 검증

* `scripts\\build_win32_x86.bat`: 성공.
* 기존 600초 aot-dynamic 관측에서 draw ordinal 66~77은 0회였으므로, 새 캡처가 실제 자료를 얻으려면 먼저 게임 상태가 draw 호출까지 진행해야 한다. 이번 빌드에서는 그 장시간 조건을 다시 실행하지 않았다.

## 다음 단계

직접 loader의 aot-dynamic 장시간 관찰에서 first-triangle 자료가 나오면, 72바이트 필드의 좌표·색상·depth·TMU 후보를 원본 실행 자료로 확정하고 전용 OpenGL draw backend 설계를 시작한다.

# Glide R2 GrVertex Observation Work Log

## Result

Added non-invasive first-call observation for `_GRDRAWTRIANGLE@12`. It records the three vertex argument pointers and 72 bytes (18 dwords) from each after guest-readable validation; unreadable pointers have a separate flag. Live output reports pointer/readable status, while the execution attempt retains raw dwords.

The existing draw gate remains an ABI-preserving stdcall no-op, so this change does not alter rendering semantics or original control flow.

## Verification

* `scripts\\build_win32_x86.bat`: succeeded.
* The prior 600-second aot-dynamic observation recorded zero calls to draw ordinals 66-77. Obtaining new vertex data therefore requires game state to first reach a draw call; this build did not repeat that long condition.

## Next step

When a long direct-loader aot-dynamic run produces first-triangle data, confirm the 72-byte coordinate, color, depth, and TMU candidates from original execution before designing the dedicated OpenGL draw backend.

## 2026-07-20 직접 관측 결과 / Direct observation result

85초 직접 loader(`pumpit1`, `aot-dynamic`, 내부 timeout disabled) 관측에서 첫 `grDrawTriangle`이 발생했습니다. 세 정점 포인터는 `0x0383C640`, `0x0383C67C`, `0x0383C6F4`였고 모두 읽을 수 있었습니다. 첫 두 포인터 간격은 60바이트이므로 기존 72바이트 가정은 이 경로에 적용할 수 없습니다. 첫 두 dword는 각각 `(288.0,329.9375)`, `(296.0,329.9375)`, `(288.0,313.9375)`로 해석됩니다. 다음 작업은 60바이트 producer layout을 추가 표본으로 확정하는 것입니다.

In an 85-second direct loader observation (`pumpit1`, `aot-dynamic`, internal timeout disabled), the first `grDrawTriangle` occurred. The three readable pointers were `0x0383C640`, `0x0383C67C`, and `0x0383C6F4`. The first two are 60 bytes apart, so the existing 72-byte assumption does not apply to this path. Their first two dwords decode as `(288.0,329.9375)`, `(296.0,329.9375)`, and `(288.0,313.9375)`. The next task is to establish the 60-byte producer layout from additional samples.