# 20260831-550 Linux x64 long-mode byte compatibility 작업 지시서

## 한국어

### 목적

Task 546 구현 순서 3단계의 전제 조건인 long-mode byte compatibility 판정기를
추가합니다. 설계는
[20260831-550](../design/20260831-550-linux-x64-long-mode-byte-compatibility.md)에
있습니다.

### 작업

- `AotLongModeCompatibility` 판정기를 전용 header/source로 추가한다. 기존
  `aot_translation_plan`에 누적하지 않는다.
- 판정 결과는 `kIdenticalBytes` / `kNeedsReencode` / `kUnsupported` 세 가지이고,
  기본값은 `kUnsupported`다.
- 설계 A(의미 변화)·B(`#UD`)·C(폭 변화) 목록을 opcode로 거부한다.
- ModRM `mod=00, rm=101`을 별도로 거부한다. opcode가 아니라 addressing form이므로
  opcode 목록으로는 잡히지 않는다.
- memory operand가 있으면 `kIdenticalBytes`를 주지 않는다.
- i386 경로의 동작을 바꾸지 않는다. 이번 단위에서 이 판정기를 호출하는 emit 경로는
  없다.

### 검증

`long_mode_compatibility` probe를 `repiu_core_probe`에 추가하고, 설계 A·B·C의 항목이
하나도 `kIdenticalBytes`를 받지 않는지 확인합니다. A의 여섯 항목은 개별 이름으로
보고합니다. Linux x64, Linux i386, Win32에서 빌드·실행합니다.

## English

### Objective

Add the long-mode byte-compatibility classifier that step 3 of Task 546's implementation
order depends on. The design is
[20260831-550](../design/20260831-550-linux-x64-long-mode-byte-compatibility.md).

### Work items

- Add an `AotLongModeCompatibility` classifier in its own header and source rather than
  accumulating it in `aot_translation_plan`.
- Three verdicts -- `kIdenticalBytes`, `kNeedsReencode`, `kUnsupported` -- with
  `kUnsupported` as the default.
- Refuse the design's lists A (meaning change), B (`#UD`), and C (width change) by opcode.
- Refuse ModRM `mod=00, rm=101` separately: it is an addressing form rather than an
  opcode, so no opcode list catches it.
- Never answer `kIdenticalBytes` for an instruction with a memory operand.
- Leave the i386 path's behaviour unchanged. Nothing in the emit path calls this
  classifier in this unit.

### Verification

Add a `long_mode_compatibility` probe to `repiu_core_probe` and confirm that not one
entry from the design's lists A, B, or C is answered `kIdenticalBytes`. Report A's six
entries under their own names. Build and run on Linux x64, Linux i386, and Win32.
