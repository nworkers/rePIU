# Task 720 설계 — Linux x64 Glide gate 직접 디스패치

## 목적

Task 719에서 Linux x64는 180초 동안 Glide 호출 242만 번을 전부 gate의 `ud2` trap과
signal로 처리했고, Win32는 252만 번을 trap 없이 직접 디스패치했다. 직접 디스패치
thunk(`AotDbtGlideGateDispatchThunk`)가 i386 어셈블리뿐이라 x64에서
`GetGlideGateDirectDispatchThunkAddress()`가 `nullptr`이기 때문이다. x64용 thunk를 만든다.

## 지금 경로

1. cache가 게스트 `call`을 lowering: 복귀 주소(게스트 주소)를 `[r15]`에 넣고 gate 주소로
   점프한다. dispatch는 gate 주소를 cache 목표로 그대로 돌려준다.
2. gate `0F 0B ordinal C3`를 64비트 모드에서 실행 → `ud2` → SIGILL.
3. signal 처리기가 `HandleGlideGateBoundary`로 Glide를 처리하고, Task 702의 재진입으로
   복귀 주소의 cache 코드로 돌아간다.

## 설계

gate 패치(`PatchGlideGatePlanForDirectDispatch`)는 그대로 쓴다. gate가 `E8 rel32(thunk)`가
되고, 64비트 모드에서 그 `call`이 host 스택에 8바이트 복귀 주소(gate+5)를 넣는다.

**thunk `RepiuLinuxX64GlideGateThunk`** (`src/platform/linux/`)

1. `pop`으로 gate+5를 꺼낸다. host RSP는 cache가 쓰던 값으로 돌아간다.
2. 게스트 레지스터, `R15D`(게스트 ESP), 플래그, gate 주소를 공용 dispatch frame에 기록한다.
3. RSP를 16바이트로 정렬하고 `fxsave64`로 x87/SSE 상태를 저장한 뒤 `fninit`과 기본
   MXCSR(`0x1F80`)을 적재한다. signal 처리기가 받던 깨끗한 FPU 상태와 같게 하고, 게스트의
   x87 상태는 호출 뒤 `fxrstor64`로 그대로 되돌린다.
4. `cld` 뒤 engine resolver를 함수 포인터로 부른다(platform 라이브러리가 engine 심볼에
   링크 의존하지 않도록 기존 return thunk와 같은 방식).
5. 실패(0)면 `int3`. 성공이면 frame에서 레지스터와 `R15D`를 되돌리고, 게스트 플래그를
   복원한 뒤 **복귀 주소를 `R14D`에 넣어 기존 `RepiuLinuxX64ReturnThunk`로 점프**한다.
   cache 목표 해석, 동적 번역, byte-identical legacy resume, 실패 시 int3는 이미 검증된 그
   경로가 맡는다.

**resolver `ResolveLinuxX64GlideGateFrame`** (engine)

i386 resolver와 같은 검사를 한다. frame으로 `GuestCpuContext`를 만들고(`Esp`=게스트 ESP,
`Eip`=gate 주소, 세그먼트는 context 값), gate를 해독하고, `HandleGlideGateBoundary`를 부른다.
`Eip`가 gate에 머물렀거나 ESP 조정이 `4 + 인자 바이트`가 아니면 실패다. 성공하면 레지스터,
ESP, 플래그(TF 제거)를 frame에 쓰고 `guest_source`에 복귀 주소를 넣는다. 통계는 i386과
같은 카운터(entry/success/terminal)를 쓴다.

**활성화** — x64에서 `GetGlideGateDirectDispatchThunkAddress()`가 thunk 주소를 돌려준다.
thunk는 비-PIE 실행 파일 안이라 4GiB 아래이고 gate(`0x095D0000` 근처)에서 rel32로 닿는다.
resolver는 게스트 진입(`CallGuestCacheEntryTimed`) 때 dispatch와 함께 설치한다.
`REPIU_AOT_DBT_GLIDE_GATE_DISPATCH=0`으로 끌 수 있다(기존 설정).

Win32 x86은 바뀌지 않는다. 새 코드는 모두 `!_WIN32 && __x86_64__` 안이다.

## 위험

* x87 스택: 호출 중 host C++ 코드가 x87을 쓸 수 있으므로 `fninit`로 비우고, 게스트 상태는
  `fxrstor64`로 되돌린다.
* 동적 번역: resolver 뒤의 목표 해석은 return thunk가 하며, 그 경로는 지금도 매 `ret`마다
  쓰인다.
* 레지스터: `R10`, `R11`, `R14`는 게스트 레지스터가 아니며 return thunk가 입력으로 쓰는
  것만 설정한다.

## 검증

* probe: x64에서 thunk 주소가 있고 4GiB 아래, gate 패치 가능
* Linux 180초 장면 기록(Task 719와 같은 조건): `ud2` boundary, 예외 처리 진입, swap 정지를
  비교하고 폴트 0 확인
* Win32 core probe와 실행 수치 비교

---

## English

### Purpose

In Task 719 Linux x64 took all 2.42 million Glide calls of a 180-second run through
the gates' `ud2` trap and a signal, while Win32 dispatched 2.52 million directly. The
direct-dispatch thunk (`AotDbtGlideGateDispatchThunk`) exists only as i386 assembly,
so `GetGlideGateDirectDispatchThunkAddress()` is `nullptr` on x64. This task builds
an x64 thunk.

### Current path

The cache lowers a guest `call` by storing the guest return address at `[r15]` and
jumping to the gate address, which dispatch returns as the cache target unchanged.
The gate `0F 0B ordinal C3` runs in 64-bit mode, `ud2` raises SIGILL, and the signal
handler runs `HandleGlideGateBoundary` and re-enters the cache at the return address
through Task 702's resume.

### Design

The gate patch (`PatchGlideGatePlanForDirectDispatch`) is reused: the gate becomes
`E8 rel32(thunk)`, and in 64-bit mode that `call` pushes an eight-byte return address
(gate+5) on the host stack.

**Thunk `RepiuLinuxX64GlideGateThunk`** (`src/platform/linux/`):

1. `pop` gate+5, restoring host RSP to the cache's value.
2. Record guest registers, `R15D` (guest ESP), flags and the gate address in the
   shared dispatch frame.
3. Align RSP to 16, save x87/SSE with `fxsave64`, then `fninit` and load the default
   MXCSR (`0x1F80`) — the clean FPU state the signal handler used to get — and put
   the guest's x87 state back with `fxrstor64` afterwards.
4. `cld`, then call the engine resolver through a function pointer, as the return
   thunk does, so the platform library has no link dependency on engine symbols.
5. On failure (0), `int3`. On success, reload registers and `R15D` from the frame,
   restore the guest flags, **put the return address in `R14D` and jump to the
   existing `RepiuLinuxX64ReturnThunk`**, which already handles cache lookup, dynamic
   translation, byte-identical legacy resume and the fail-closed int3.

**Resolver `ResolveLinuxX64GlideGateFrame`** (engine) makes the i386 resolver's
checks: it builds a `GuestCpuContext` from the frame (`Esp` = guest ESP, `Eip` = gate
address, segments from the context), decodes the gate and calls
`HandleGlideGateBoundary`. It fails if `Eip` stayed on the gate or ESP did not move by
`4 + argument bytes`. On success it writes registers, ESP and flags (TF cleared) back
and puts the return address in `guest_source`. It uses the i386 counters
(entry/success/terminal).

**Enabling**: on x64 `GetGlideGateDirectDispatchThunkAddress()` returns the thunk.
The thunk is in the non-PIE executable, below 4 GiB, and within rel32 of the gates
(around `0x095D0000`). The resolver is installed with the dispatch at guest entry
(`CallGuestCacheEntryTimed`). `REPIU_AOT_DBT_GLIDE_GATE_DISPATCH=0` turns it off (the
existing setting).

Win32 x86 does not change; all new code is inside `!_WIN32 && __x86_64__`.

### Risks

* x87 stack: host C++ code may use x87 during the call, so it is emptied with
  `fninit` and the guest's state restored with `fxrstor64`.
* Dynamic translation: target lookup after the resolver is the return thunk's,
  already used on every `ret`.
* Registers: `R10`, `R11` and `R14` are not guest registers; only what the return
  thunk takes as input is set.

### Verification

* Probe: on x64 the thunk address exists, is below 4 GiB, and the gate patch applies.
* A 180-second Linux scene recording under Task 719's conditions: compare `ud2`
  boundaries, exception dispatches and swap stalls, with no faults.
* Win32 core probe and run figures compared.
