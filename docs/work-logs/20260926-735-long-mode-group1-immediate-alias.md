# Task 735 작업 로그 — long mode에서 `82 /r ib`를 `80 /r ib`로

설계: [20260926-735](../design/20260926-735-long-mode-group1-immediate-alias.md)
작업 지시: [20260926-735](../work-orders/20260926-735-long-mode-group1-immediate-alias.md)

## 요약

Linux x64에서 pumpitea가 시작하지 못하던 원인을 찾아 고쳤습니다. AOT 이미지의 decode 검증이
항목 하나를 거절했는데, guest `0x011074D8`의 `82 68 10 01`(`sub byte ptr [eax+0x10], 1`)이었습니다.
opcode `82`는 `80` group-1 byte 연산의 별칭이고 long mode에서는 #UD입니다. long-mode 경로가 `82`를
`80` 표기로 판단하고 방출하게 한 뒤 이미지가 만들어지고 게임이 실행됩니다.

## 과정

1. 사용자 로그: `Failed to build requested AOT execution image: AOT translation plan is ready /
   emitted code cache failed decode verification`. WSL에서 그대로 재현했습니다(수 초).
2. decode 검증은 실패 개수만 셌습니다. `AotDecodeFailureSample`(최대 8개)을 이미지에 두고 로더가
   빌드 실패 시 출력하게 했습니다. 결과:

   ```
   AOT decode failures total/sampled: 1/1
   AOT decode failure guest=0x011074D8 kind=0 guest_bytes=82681001 cache=0x00008A4A
     emitted_len=5 decoded_bytes=0 decoded_instructions=0 expected_instructions=1 emitted=6782681001
   ```
3. long-mode 분류기 `IsInvalidInLongMode` 목록에 `82`가 없었습니다. 메모리 형식은 주소 크기
   lowering만 받아 `67 82 …`로 방출되었습니다. 레지스터 형식이었다면 "동일 바이트"로 판정되어
   그대로 복사되었을 것입니다.
4. 수정: `ClassifyLongModeBytes`와 `LowerLongModeBytes`가 opcode `82`(Zydis `raw.prefix_count`
   위치)를 `80`으로 바꾼 사본으로 판단·방출합니다. 사본이 동일 바이트면 새 lowering
   `kGroup1ImmediateAlias`, 다른 lowering이 필요하면 그 lowering을 사본에 적용합니다.

## 검증

| 검증 | 결과 |
|---|---|
| core probe `long_mode_group1_immediate_alias` | `82 C0 01` → `80 C0 01`(long mode `add al,1`), `82 68 10 01` → `67 80 68 10 01`(`sub byte [eax+0x10],1`, 주소 폭 32) 모두 통과 |
| Linux x64 core probe 전체 | 31/31, 실패 0 |
| pumpitea, WSL Linux x64 | 이미지 418,364 bytes. 88초 실행, 폴트 0, 1,582 frame, SERVICE·패드 입력에 YMZ 효과음, `AUDIO\*.AUD` open |
| pumpit2a 회귀, 20초 | 이미지 크기 358,152 bytes로 동일, 폴트 0, 1,555 frame |
| Win32 x86 Debug 빌드 + core probe | 빌드 성공, core probe 29/29(새 case 포함) |

pumpitea 실행 중의 SERVICE·패드 입력은 제가 넣은 것이 아닙니다. WSLg 창이 사용자 화면에 떠서
사용자가 누른 것으로 보입니다. 로그는 `build/task735-pumpitea-run1.err.log`입니다.

## 관찰 — 입력 스캔의 trap 부하 (고치지 않음)

pumpitea의 입력 스캔(`0x01028E30`)은 JAMMA를 읽기 전에 `in ax, dx`를 200번 반복하는 지연 루프를
돕니다. 타이머 tick마다 한 번 돌아 88초에 포트 읽기 4,052,600회(전체의 99%)였고, 전부 arena에서
trap과 HLE 재진입으로 실행되었습니다(cache 0). breakpoint 예외는 88초에 505만 회였습니다. 원본
동작 그대로이며 결과도 맞지만 비용이 큽니다. 체감 속도에 주는 영향과 ISR 코드가 AOT port I/O
direct dispatch를 타지 않는 이유는 조사하지 않았습니다.

## 남은 것

1. 위 trap 부하의 측정과 개선.
2. 곡 선택·플레이까지의 진행은 확인하지 않았습니다.

---

# English

# Task 735 work log — `82 /r ib` becomes `80 /r ib` in long mode

Design: [20260926-735](../design/20260926-735-long-mode-group1-immediate-alias.md)
Work order: [20260926-735](../work-orders/20260926-735-long-mode-group1-immediate-alias.md)

## Summary

pumpitea did not start on Linux x64. The AOT image's decode check rejected one entry, guest `0x011074D8`
`82 68 10 01` (`sub byte ptr [eax+0x10], 1`). Opcode `82` is the alias of the `80` group-1 byte
operations and raises #UD in long mode. With the long-mode path judging and emitting `82` as its `80`
spelling, the image builds and the game runs.

## Steps

1. The user's log read `Failed to build requested AOT execution image: AOT translation plan is ready /
   emitted code cache failed decode verification`; it reproduced on WSL within seconds.
2. The decode check only counted failures. The image now carries up to eight
   `AotDecodeFailureSample`s and the loader prints them when the build fails; the output above named
   the one entry.
3. The long-mode classifier's `IsInvalidInLongMode` list lacked `82`. The memory form received only the
   address-size lowering and was emitted as `67 82 …`; a register form would have been judged identical
   and copied verbatim.
4. Fix: `ClassifyLongModeBytes` and `LowerLongModeBytes` judge and emit a copy with the opcode byte
   (located by Zydis `raw.prefix_count`) set to `80`. An identical copy gets the new lowering
   `kGroup1ImmediateAlias`; a copy that needs another lowering gets that lowering applied.

## Verification

| Check | Result |
|---|---|
| core probe `long_mode_group1_immediate_alias` | `82 C0 01` → `80 C0 01` (`add al,1` in long mode) and `82 68 10 01` → `67 80 68 10 01` (`sub byte [eax+0x10],1`, 32-bit address width) both pass |
| Whole Linux x64 core probe | 31/31, zero failures |
| pumpitea, WSL Linux x64 | 418,364-byte image; 88-second run, no faults, 1,582 frames, YMZ sound effects on SERVICE and pad input, `AUDIO\*.AUD` opened |
| pumpit2a regression, 20 s | image size unchanged at 358,152 bytes, no faults, 1,555 frames |
| Win32 x86 Debug build + core probe | build succeeded, core probe 29/29 (new case included) |

The SERVICE and pad input during the pumpitea run was not mine; the WSLg window appeared on the user's
desktop and the user appears to have pressed them. The log is `build/task735-pumpitea-run1.err.log`.

## Observation — the input scan's trap load (not fixed)

pumpitea's input scan (`0x01028E30`) runs a delay loop of 200 `in ax, dx` before reading JAMMA. It runs
once per timer tick: 4,052,600 port reads in 88 s (99% of all), all executed in the arena through a
trap and HLE re-entry (cache 0), with 5.05 million breakpoint exceptions in 88 s. This is the original
behaviour and the results are correct, but the cost is high. Its effect on perceived speed, and why the
ISR code does not take AOT port I/O direct dispatch, were not investigated.

## What remains

1. Measuring and reducing the trap load above.
2. Progress to song select and play was not checked.
