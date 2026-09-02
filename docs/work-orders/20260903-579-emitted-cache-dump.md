# 작업 지시 20260903-579 — 방출된 캐시 바이트 덤프

설계: [20260903-579](../design/20260903-579-emitted-cache-dump.md)

## 변경 대상

| 파일 | 변경 |
|---|---|
| `src/tools/instruction_census/main.cpp` | `--cache <offset>` 옵션과 덤프 |
| `docs/analysis/linux-port-frontier.md` | 3.25절 |

## 구현 단계

1. 인자 처리에 `--cache <offset>`를 더합니다. 16진수를 받습니다.
2. long-mode 이미지를 만든 뒤, 옵션이 있으면 그 offset을 덮는 address-map 항목을
   찾아 앞뒤 창을 함께 찍습니다.
3. 항목마다 cache offset·emitted length·guest address·kind·guest bytes·emitted
   bytes를 찍습니다.
4. 옵션이 없으면 기존 출력이 **한 글자도** 달라지지 않아야 합니다.

## 검증 절차

1. `--cache 0x5`를 `build/runtime_mounts/pumpit2a/PIU/PIU.EXE`에 대해 실행하고
   결과를 기록합니다.
2. 같은 실행에서 `agrees=true` 유지.
3. 옵션 없는 실행이 이전 census 숫자와 동일.
4. Linux x64 `repiu_core_probe`, i386, Win32 회귀.

## 완료 조건

- cache+5에 무엇이 있는지 관측으로 기록됩니다.
- Task 578 추론 중 어느 고리가 틀렸는지 기록됩니다.
- 작업 로그와 frontier 3.25절.

---

# Work order 20260903-579 — Dumping the emitted cache bytes

Design: [20260903-579](../design/20260903-579-emitted-cache-dump.md)

## Files to change

| File | Change |
|---|---|
| `src/tools/instruction_census/main.cpp` | The `--cache <offset>` option and the dump |
| `docs/analysis/linux-port-frontier.md` | Section 3.25 |

## Implementation steps

1. Add `--cache <offset>` to argument handling, accepting hex.
2. After the long-mode image is built, when the option is present find the
   address-map entry covering that offset and print a window around it.
3. Per entry print cache offset, emitted length, guest address, kind, guest
   bytes, and emitted bytes.
4. Without the option the existing output must be **byte for byte** unchanged.

## Verification

1. Run `--cache 0x5` against
   `build/runtime_mounts/pumpit2a/PIU/PIU.EXE` and record the result.
2. `agrees=true` still holds in the same run.
3. A run without the option matches the previous census numbers.
4. Linux x64 `repiu_core_probe`, i386, and Win32 regressions.

## Completion criteria

- What sits at cache+5 is recorded as an observation.
- Which link of Task 578's reasoning was wrong is recorded.
- A work log and frontier section 3.25.
