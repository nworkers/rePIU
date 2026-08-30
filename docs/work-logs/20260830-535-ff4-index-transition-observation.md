# 20260830-535 FF /4 index 전환 순서 관측 작업 로그

## 작업 목적

Task 534 histogram에서 발견한 복수 index 값의 실제 순서를 확인하여 `pumpipx3`과 `pumpit1`의
FF /4 dispatch 선택 패턴을 비교했습니다. 원본 guest code와 실행 상태는 변경하지 않았습니다.

## 변경 내용

* FF /4 site와 hotspot에 최대 32개 ordered transition slot을 추가했습니다.
* 각 transition에 이전/현재 SIB index register와 value를 저장했습니다.
* live reporter에 total transition, stored slot, overflow counter와 transition trace를
  추가했습니다.
* synthetic target-attribution probe에 transition count, slot, overflow, from/to 검증을
  추가했습니다.

## 검증

Win32 x86 Debug와 Linux i386 Release를 빌드했습니다.

```text
cmake --build build\win32_x86_debug --config Debug --target repiu_aot_probe --parallel 1
wsl.exe -d Ubuntu-24.04 -- cmake --build /mnt/e/MYWORK/Projects/rePIU/build/linux_i386 --parallel 2
```

두 빌드 모두 exit code 0이었습니다. 기존 code-page, macro-redefinition, format 경고가
출력되었으나 Task 535 변경으로 인한 error는 없었습니다.

Win32 synthetic probe:

```text
build\win32_x86_debug\Debug\repiu_aot_probe.exe roms\pumpipx3\PIU\PIU.EXE
aot_ff_boundary_target_attribution=true
aot_boundary_opcode_census_all=true
```

## 동일 조건 runtime 측정

두 실행에 다음 환경을 적용했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
```

결과 요약:

| title/site | sample | transition result | observed order |
| --- | --- | --- | --- |
| `pumpipx3` / `0x010EF6DE` | #1 | `tx=21`, `ts=21`, `to=0` | EDX `0→7→0→7...` |
| `pumpipx3` / `0x010EF6DE` | #5 | `tx=121`, `ts=32`, `to=89` | stored prefix EDX `0→7→0→7...` |
| `pumpit1` / `0x010F1DD7` | #1 | `tx=8`, `ts=8`, `to=0` | EAX `3→2→3→2...` |
| `pumpit1` / `0x010F1DD7` | #5 | `tx=13`, `ts=13`, `to=0` | EAX `3→2→3→2...` |

At both sites all target reads were resolved; unresolved, truncated, unsupported, and unreadable
counters were zero. Pumpipx3's early trace and all pumpit1 traces fit within the 32-slot bound.
The 89 late pumpipx3 transitions were intentionally counted as overflow, so their individual
order is not claimed.

Both 60-second processes reached timeout teardown and ended with `recovered=0, stopped=0` and
process exit 1. The live FF evidence was emitted before teardown; this is a cleanup limitation,
not a target-resolution failure.

## 정적 상관과 결론

Pumpipx3 static table `0x010EF65C` maps index 0 to `0x010EF6E6` and index 7 to `0x010EF8E9`.
The bounded bytes around the site include `xor edx, edx` at `0x010EF6DA`, `mov dl, bl` at
`0x010EF6DC`, and the preceding `mov bl, [eax]` at `0x010EF6CF`. The observed transition
sequence therefore represents alternating table-entry selection. It is not evidence of one
fixed pointer's target dword changing in place for this window.

Pumpit1's EAX transitions select the previously resolved index-3 pair
`0x010F1D9B -> 0x010F1CFD` and index-2 pair `0x010F1D97 -> 0x010F1CF4`.

The index transition order is now confirmed, but the complete EDX/EAX producer chains, guest
writer, late performance-drop causality, and pure resolved-target cycle cost remain unresolved.
Pure target timing is deferred because the current FF-boundary hook includes VEH/handler and
subsequent guest work. A future full late pumpipx3 sequence would require a larger bounded trace
or streaming/hash observer.

## English summary

Task 535 recorded the ordered transitions of resolved FF /4 SIB indexes without modifying guest
execution. Each site/hotspot stores up to 32 transitions and reports total, stored, and overflow
counts. Both platform builds passed, and the synthetic attribution/census probe returned true.

The dominant pumpipx3 site alternated EDX 0 and 7. It recorded 21 complete transitions at sample
#1 and 121 total transitions at sample #5, of which 32 were stored and 89 overflowed. Pumpit1
alternated EAX 3 and 2 and stored all 8 early and 13 late transitions. All target reads resolved.
Static table correlation maps pumpipx3 index 0 and index 7 to different targets, rejecting the
same-pointer target mutation explanation for the observed prefix. Producer chains, guest writer,
late-drop causality, cleanup recovery, and pure target cycles remain open.
