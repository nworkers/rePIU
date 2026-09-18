# Task 717 설계 — loader 데이터 선택자도 flat으로 fold

## 목적

Task 716 이후 Linux x64에서 safe point 주입을 켜면 27–28초에 SIGSEGV로 죽는다. GL
텍스처 바인드가 읽는 `ctx->Shared->TexObjects`가 `0xF186F186`로 덮여 있다. 누가
덮는지 찾아 고친다.

## 조사 방법

고정 주소 peek로는 한 번에 일어나는 fill의 주체를 알 수 없어 진단 세 가지를 더한다.
모두 설정하지 않으면 아무 일도 하지 않는다.

| 변수 / 출력 | 하는 일 |
|---|---|
| `REPIU_LIVE_GUEST_SCAN=<runtime offset>,<bytes>,<dword>` | 매초 범위 안에서 dword의 개수와 처음 몇 개의 연속 구간을 기록 |
| `[repiu-fault-stack]` | Linux 처리되지 않은 폴트 뒤에 게스트 스택 64 dword |
| `REPIU_LINUX_X64_DATA_WATCH=<address>` | `perf_event_open` 하드웨어 쓰기 감시점(4바이트). `SIGTRAP`/`TRAP_PERF`로 받아 기록하고 계속 실행 |

## 원인

감시점이 쓰는 명령을 잡았다: Watcom `sprintf`의 출력 콜백(`0x010F0DF5`,
`*p++ = c`)이 `86 F1`을 한 바이트씩 쓰고 있었고, 호출자는 `__prtf`의 숫자 복사
루프(`0x010F35B3`, `mov dl, es:[ebx]`)였다.

```text
010F397E  숫자 변환: 스택 버퍼 주소와 DS 값을 eax:dx로 반환
010F345D  mov es, dx
010F35D1  mov dl, es:[ebx]     ; 숫자 문자열 복사
```

같은 지점의 레지스터(arrival probe):

| | Win32 | Linux x64 |
|---|---|---|
| `ebx`(버퍼) | `0x0458CA90`(스택) | `0x0158CA98`(스택) |
| `edx`(ES로 갈 값) | **`0x002B`** host flat 선택자 | **`0x0024`** loader 선택자 |

loader는 선택자 `0x0024`를 object 2(base `0x01010000`)에 묶는다. Linux에서 ES
override fold가 그 base를 더해 `0x0259CA98`의 텍스처 픽셀을 읽었고, NUL이 없어
`0x5353`바이트 넘게 복사하면서 스택 버퍼 위의 호출자 프레임과 힙(`Shared`)을 덮었다.

## 설계

게스트의 암묵적 데이터 접근은 두 host 모두 host의 flat 세그먼트에서 선형 주소로
실행된다. 따라서 **loader의 32비트 object 선택자를 명시적으로 쓰는 override도
base 0**이어야 한다. Task 712가 SS에 대해 같은 이유로 초기 스택 선택자(`0x0034`)를
flat으로 두었다.

* `ApplyFlatSegmentFolds(flat_stack, flat_data, table)` — ES, SS, DS, FS, GS 모든
  항목에서 selector가 초기 스택 선택자이거나 초기 데이터 선택자(`guest_ds` 초기값,
  `0x0024`)이면 fold base를 0으로 한다. CS는 shadow가 없어 건드리지 않는다.
* 다른 선택자(object 3의 16비트 `0x002C`, 게스트가 만든 스택 `B4`)는 descriptor
  base를 유지한다.
* `ThreadContext::flat_data_selector`에 초기 `guest_ds`를 기록한다.

Win32에서는 게스트가 DS를 읽으면 `0x2B`를 얻으므로 이 경로는 거의 바뀌지 않을
것으로 본다. 실행으로 확인한다.

## 검증

* probe: 표 전체에서 ES/DS의 `0x0024`와 FS의 `0x0034`는 0, 게스트 스택과 object 3은
  유지
* Linux 주입 on: fill과 크래시 소멸, 긴 실행 안정성
* Linux 주입 off: 이전과 같음
* Win32: core probe, 30초 실행의 시계·safe point·selector guard 수치 비교

---

## English

### Purpose

After Task 716, Linux x64 with safe-point injection on dies at 27–28 seconds on a
SIGSEGV: `ctx->Shared->TexObjects`, read by a GL texture bind, has been overwritten
with `0xF186F186`. Find what overwrites it and fix it.

### Method

A fixed-address peek cannot name the author of a one-shot fill, so three
diagnostics were added, all inert when unset:
`REPIU_LIVE_GUEST_SCAN=<runtime offset>,<bytes>,<dword>` logs each second the
dword's count and first runs in the range; `[repiu-fault-stack]` dumps 64 guest
stack dwords after an unhandled Linux fault; and
`REPIU_LINUX_X64_DATA_WATCH=<address>` arms a four-byte `perf_event_open` hardware
write watchpoint, taken as `SIGTRAP`/`TRAP_PERF`, logged, and resumed.

### Cause

The watchpoint caught the writer: Watcom `sprintf`'s output callback (`0x010F0DF5`,
`*p++ = c`) writing `86 F1` a byte at a time, called from `__prtf`'s number copy
loop (`0x010F35B3`, `mov dl, es:[ebx]`). `0x010F397E` returns the stack buffer and
the DS value in `eax:dx`, and `0x010F345D` loads that into ES. At that point Win32
has `ebx=0x0458CA90` and `edx=0x002B` (the host's flat selector); Linux has
`ebx=0x0158CA98` and `edx=0x0024` (a loader selector). The loader binds `0x0024` to
object 2 at base `0x01010000`; the Linux ES fold added it, read texture pixels at
`0x0259CA98`, found no NUL, and copied over `0x5353` bytes across the caller frames
above the stack buffer and into the heap (`Shared`).

### Design

The guest's implicit data accesses run on the host's flat segments with linear
addresses on both hosts, so **an explicit override naming one of the loader's
32-bit object selectors must fold base 0 as well** — the reason Task 712 gave the
initial stack selector (`0x0034`) for SS.

* `ApplyFlatSegmentFolds(flat_stack, flat_data, table)` sets the fold base to 0 in
  every entry — ES, SS, DS, FS, GS — whose selector is the initial stack selector or
  the initial data selector (the initial `guest_ds`, `0x0024`). CS has no shadow and
  is untouched.
* Other selectors (object 3's 16-bit `0x002C`, the guest-built stack `B4`) keep
  their descriptor base.
* `ThreadContext::flat_data_selector` records the initial `guest_ds`.

On Win32 the guest reads `0x2B` for DS, so little should change there; the run
checks it.

### Verification

* Probe: across a whole table, `0x0024` in ES/DS and `0x0034` in FS go to 0; the
  guest stack and object 3 keep their bases.
* Linux with injection on: the fill and crash gone; stability over a longer run.
* Linux with injection off: unchanged.
* Win32: core probe, and the clock, safe-point and selector-guard figures of a
  30-second run compared.
