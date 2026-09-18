# Task 710 작업 지시 — Linux x64 long-mode timer safe point

설계: [20260918-710](../design/20260918-710-linux-x64-timer-safe-points.md)

## 범위

long-mode emission 경로가 timer safe point를 심게 하고, 그 safe point가 long
mode에서 바른 주소를 읽게 한다. i386 방출 바이트, safe point 정책, 게스트 코드,
타이머 전달 정책은 바꾸지 않는다.

## 단계

1. `src/runtime/aot_code_cache.cpp`
   * `EmitTimerSafePoint`가 long mode에서 `83 3C 25 <disp32> 00`을,
     i386에서 기존 `83 3D <disp32> 00`을 방출하도록 한다.
     `request_address_offset`은 방출한 형식에 맞게 기록한다.
   * long-mode 분기가 backward edge에서 `EmitTimerSafePoint`를 부르도록 한다.
     판정은 기존 `IsBackwardEdge`를 그대로 쓴다.
2. `src/engine/aot_code_cache.cpp`
   * `timer_safe_point_request`를 code cache 예약 안의 고정 위치에 둔다.
   * `ResolveAotTimerSafePoints`의 4 GiB 검사는 남긴다.
3. `src/tools/aot_probe/long_mode_emission_probe.cpp`
   * backward edge 계획에서 long mode와 i386의 site 수가 같은지
   * long mode 방출 바이트가 `83 3C 25`인지
4. 문서: 작업 로그, `docs/analysis/linux-port-frontier.md`,
   필요하면 `docs/kb/x86-32bit-encodings-in-long-mode.md`에 ModRM `0x3D` 항목.

## 검증

* Linux x64 Debug 빌드, core probe 전체
* Win32 x86 Debug 빌드, core probe 전체, site 수 `1067` 유지
* Linux `pumpit2a` 10초: `handled DOS read count`
* Linux `pumpit2a` 30초: `AOT timer safe points enabled/sites`,
  틱 `dropped`/`remaining`, Glide gate #51

## 완료 조건

* Linux x64의 `AOT timer safe points enabled/sites`가 `true/0`이 아니다.
* 10초 실행의 `handled DOS read count`가 769,639에서 크게 떨어진다.
* Win32의 site 수와 방출 바이트가 그대로다.
* 새 probe 사례가 수정 전 구현에서 실패하고 수정 후 통과한다.

## 완료 조건이 아닌 것

게스트가 `PIU.BIN`에 쓰레기 offset으로 seek하는 것은 두 host 공통이고 이 작업의
대상이 아니다. 텍스처 업로드 블록까지 진행하는지는 예측이며, 실행으로 확인하되
이 작업의 판정 기준은 위 네 줄이다.

---

## English

Design: [20260918-710](../design/20260918-710-linux-x64-timer-safe-points.md)

### Scope

Make the long-mode emission path plant timer safe points, and make those safe
points read the right address under long mode. The i386 emitted bytes, the
safe-point policy, guest code, and timer delivery policy are unchanged.

### Steps

1. `src/runtime/aot_code_cache.cpp`: have `EmitTimerSafePoint` emit
   `83 3C 25 <disp32> 00` under long mode and the existing
   `83 3D <disp32> 00` on i386, recording `request_address_offset` to match the
   form emitted; and have the long-mode branch call it on a backward edge,
   reusing the existing `IsBackwardEdge` test.
2. `src/engine/aot_code_cache.cpp`: place `timer_safe_point_request` at a fixed
   location inside the code-cache reservation, keeping the 4 GiB check.
3. `src/tools/aot_probe/long_mode_emission_probe.cpp`: a backward-edge plan
   must give long mode the same site count as i386, and the long-mode bytes
   must be `83 3C 25`.
4. Documents: the work log, `docs/analysis/linux-port-frontier.md`, and a
   ModRM `0x3D` entry in `docs/kb/x86-32bit-encodings-in-long-mode.md` if it
   is not already covered.

### Verification

* Linux x64 and Win32 x86 Debug builds and every core-probe group, with the
  Win32 site count staying at `1067`
* `handled DOS read count` on a 10-second Linux `pumpit2a` run
* `AOT timer safe points enabled/sites`, the dropped and remaining tick counts,
  and Glide gate #51 on a 30-second run

### Done when

* Linux x64 no longer reports `true/0` for timer safe points.
* The 10-second run's `handled DOS read count` falls sharply from 769,639.
* Win32's site count and emitted bytes are unchanged.
* The new probe case fails against the pre-fix implementation and passes after.

### Not a completion condition

The guest's garbage-offset seek on `PIU.BIN` happens on both hosts and is out of
scope. Reaching the texture-upload block is a prediction to be checked by
running it, not the criterion.
