# 작업 기록 20260903-575 — Linux x64 `repiu` 링크

설계: [20260903-575](../design/20260903-575-linux-x64-repiu-link.md) ·
작업 지시: [20260903-575](../work-orders/20260903-575-linux-x64-repiu-link.md)

## 구현

`src/platform/linux/guest_stack_recover_x64.S`를 만들고 두 심볼을 각각 `ud2`로
정의했습니다. CMake의 `if(CMAKE_SIZEOF_VOID_P GREATER 4)` 블록에 넣었으므로 i386
빌드에는 들어가지 않습니다.

`ret`가 아니라 `ud2`인 이유는 설계 결정 1에 있습니다 — 도달 자체가 결함이므로,
i386 동작을 흉내 내 "복구된 것처럼" 보이게 두지 않습니다.

## 결과 — 실행 파일이 생겼고, 그것이 새 사실 두 개를 알려 줬습니다

```text
build/linux_x64_repiu/repiu: ELF 64-bit LSB executable, x86-64, ...
```

미해결 심볼은 GL 함수들(동적 링크)뿐입니다. **엔진 심볼 미해결 0.**

### 정정 1 — 설계가 걱정한 주소 잘림은 이 구성에서 일어나지 않습니다

설계는 `context->Eip`가 32비트라 x64 함수 주소가 잘리고, `REG_RIP`와 merge할 때
상위 절반이 어긋나면 쓰레기가 된다고 적었습니다. **일반론으로는 맞지만 이 빌드에서는
일어나지 않습니다.**

```text
0000000040 1b2ed6 T RecoverGuestStackException
00000000401b2ed8 T RecoverHostStackException
```

`repiu_link_linux_engine`이 `-no-pie`와 `-Wl,-Ttext-segment=0x40000000`을 **UNIX
전체에** 적용하므로(Task 503d), x64 text도 0x40000000대에 있습니다. 주소가 32비트에
들어가고 상위 절반이 0이므로 잘림이 무손실입니다.

설계에 적은 위험은 **가정이었고 측정으로 반증됐습니다.** `ud2`를 택한 판단은
여전히 유효하지만, 그 근거 두 개 중 하나는 이 구성에서 성립하지 않습니다.

### 정정 2 — 실행이 드러낸 것: 엔진은 long-mode 방출을 켜지 않습니다

실행 파일을 실제로 돌렸습니다. **로더가 x64에서 동작합니다.**

```text
[info] [loader] DOS virtual filesystem message: DOS virtual filesystem is ready
[info] [loader] Win32 relocated image placement result: placed
[info] [loader] Win32 relocated image placed base: 0x01000000
[info] [loader] Win32 relocated image copied objects: 4
[info] [loader] Win32 relocated selector binding count: 4
[error] [loader] Failed to place requested AOT code cache:
                 AOT timer safe-point request is unavailable
[info] [loader] pumpit2a exited with code 1; returning to the launcher
```

DOS 가상 파일시스템, LE 이미지 재배치(0x01000000, object 4개 복사·보호),
selector binding 4개까지 전부 x64에서 성공합니다. **Task 544의 guest entry
울타리에 닿기도 전에** AOT code cache 배치에서 멈춥니다.

멈춘 이유를 따라가면 이 세션의 가장 중요한 발견이 나옵니다.

`ResolveAotTimerSafePoints`는 `timer_safe_point_sites`가 비어 있으면 바로
성공합니다. 실패했다는 것은 **site가 있다는 뜻**입니다. 그런데 Task 553은
long-mode 이미지가 timer safe point를 내지 않는다고 적어 두었습니다 — 그것은
손으로 쓴 32비트 `pushfd`/`popfd` 시퀀스이기 때문입니다.

모순의 답은 하나입니다.

```
$ grep -rn "long_mode_emission" src/engine/
(결과 없음)
```

`AotCodeCacheBuildOptions::enable_long_mode_emission`은 기본값이 `false`이고,
**엔진 어디에서도 켜지 않습니다.** `true`로 두는 곳은 census와 probe뿐입니다.

즉 **x64에서 로더는 i386 방식 이미지를 만듭니다.** 게스트의 32비트 바이트를 그대로
복사하고, 손으로 쓴 32비트 timer safe point와 inline cache를 냅니다. 실패한 것은
그 safe point의 request 주소가 4 GiB 위에 있기 때문입니다 —
`AotCodeCachePlacement`는 text가 아니라 힙/스택에 있으므로 0x40000000 특례를 받지
않습니다.

**Tasks 550~574가 만든 long-mode emitter 전체가 엔진에 연결되어 있지 않습니다.**
census와 probe에서만 도달합니다.

## 이것이 3.20절 표를 바꿉니다

3.20절은 남은 작업을 항목 1(링크) → 항목 2(`Eip`/`Esp` 의미) 순으로 적었습니다.
그 사이에 **항목 0**이 있었습니다.

| # | 항목 | 3.20 시점 | 지금 |
|---|---|---|---|
| 0 | 엔진이 long-mode 방출을 켜는 것 | **몰랐음** | **미해결 — 다음 단위** |
| 1 | 심볼 두 개 | 링크 실패 | **해결** |
| 2 | fault 경로 `Eip`/`Esp` 의미 | 틀린 값 | 미해결 |
| 3 | guest entry (`return 4`) | 울타리 | 미해결 |

항목 0을 먼저 하지 않으면 항목 2·3을 고쳐도 x64는 i386 바이트를 실행하려 듭니다.
그리고 항목 0을 켜는 순간 timer safe point가 사라지므로, 지금 막고 있는 그 오류도
함께 없어질 가능성이 있습니다 — 확인해야 할 가설입니다.

정적 census로는 이 사실에 닿을 수 없었습니다. 실행 파일을 만든 것이 이 단위의
성과이고, 첫 실행이 곧바로 그것을 보여 줬습니다.

## 검증

| 항목 | 결과 |
|---|---|
| x64 링크 | **성공**, 엔진 심볼 미해결 0, ELF 64-bit x86-64 |
| x64 실행 | **성공**, 로더·DOS FS·LE 배치 동작, AOT 배치에서 정지, exit 0 |
| i386 `repiu` 링크 | 성공 (9,053,996 바이트) |
| i386 `repiu_core_probe` | 19/19, failures 0 |
| Win32 `repiu_aot_probe` | `_all=true` 41개, `_all=false` 0개 |

새 파일은 x64 게이트 안에 있으므로 i386 빌드에 들어가지 않지만, `CMakeLists.txt`가
공용이므로 두 회귀를 모두 확인했습니다.

## 아직 확인하지 않음

- x64 `repiu`를 **다른 ROM 세트**로 돌려 보지 않았습니다. 관측은 `pumpit2a`
  한 번입니다.
- 항목 0을 켰을 때 무엇이 나올지는 가설이고 재지 않았습니다.

---

# Work log 20260903-575 — Linking `repiu` on Linux x64

Design: [20260903-575](../design/20260903-575-linux-x64-repiu-link.md) ·
work order: [20260903-575](../work-orders/20260903-575-linux-x64-repiu-link.md)

## Implementation

`src/platform/linux/guest_stack_recover_x64.S` defines both symbols as `ud2`. It
sits in CMake's `if(CMAKE_SIZEOF_VOID_P GREATER 4)` block, so it does not enter
the i386 build.

`ud2` rather than `ret` for design decision 1's reason: arriving is itself the
defect, so it is not left to imitate i386 and look recovered.

## Result — an executable, and it produced two new facts

```text
build/linux_x64_repiu/repiu: ELF 64-bit LSB executable, x86-64, ...
```

The only unresolved symbols are GL functions, resolved dynamically. **Zero
unresolved engine symbols.**

### Correction 1 — the address truncation the design worried about does not occur here

The design wrote that `context->Eip` being 32 bits truncates an x64 function
address, and that merging into `REG_RIP` yields rubbish when the halves differ.
**True in general, but it does not happen in this build.**

```text
00000000401b2ed6 T RecoverGuestStackException
00000000401b2ed8 T RecoverHostStackException
```

`repiu_link_linux_engine` applies `-no-pie` and
`-Wl,-Ttext-segment=0x40000000` to **all of UNIX** (Task 503d), so the x64 text
also sits at 0x40000000. The addresses fit in 32 bits with a zero top half, and
the truncation is lossless.

The hazard in the design was **an assumption, and measurement refuted it.** The
choice of `ud2` still stands, but one of its two stated reasons does not hold in
this configuration.

### Correction 2 — what running revealed: the engine never enables long-mode emission

The executable was actually run. **The loader works on x64.**

```text
[info] [loader] DOS virtual filesystem message: DOS virtual filesystem is ready
[info] [loader] Win32 relocated image placement result: placed
[info] [loader] Win32 relocated image placed base: 0x01000000
[info] [loader] Win32 relocated image copied objects: 4
[info] [loader] Win32 relocated selector binding count: 4
[error] [loader] Failed to place requested AOT code cache:
                 AOT timer safe-point request is unavailable
[info] [loader] pumpit2a exited with code 1; returning to the launcher
```

The DOS virtual filesystem, the LE image relocation (0x01000000, four objects
copied and protected), and four selector bindings all succeed on x64. It stops
at AOT code cache placement — **before ever reaching Task 544's guest-entry
fence.**

Following why it stopped produces this session's most consequential finding.

`ResolveAotTimerSafePoints` returns success immediately when
`timer_safe_point_sites` is empty. Failing means **sites exist**. But Task 553
recorded that a long-mode image emits no timer safe point, because that is a
hand-built 32-bit `pushfd`/`popfd` sequence.

There is only one resolution:

```
$ grep -rn "long_mode_emission" src/engine/
(no results)
```

`AotCodeCacheBuildOptions::enable_long_mode_emission` defaults to `false` and
**nothing in the engine ever sets it.** The only places that set it true are the
census and the probes.

So **on x64 the loader builds an i386-style image**: the guest's 32-bit bytes
copied verbatim, with hand-built 32-bit timer safe points and inline caches. The
failure is that such a safe point's request address is above 4 GiB —
`AotCodeCachePlacement` lives on the heap or stack, not in the text that gets
the 0x40000000 treatment.

**The entire long-mode emitter Tasks 550–574 built is not wired into the
engine.** It is reachable only from the census and the probes.

## This changes section 3.20's table

Section 3.20 ordered the remaining work as item 1 (link) then item 2
(`Eip`/`Esp` meaning). There was an **item 0** between them.

| # | Item | At 3.20 | Now |
|---|---|---|---|
| 0 | The engine enabling long-mode emission | **not known** | **open — the next unit** |
| 1 | Two symbols | link failure | **done** |
| 2 | The fault path's `Eip`/`Esp` meaning | wrong values | open |
| 3 | Guest entry (`return 4`) | fenced | open |

Without item 0, fixing items 2 and 3 still leaves x64 trying to execute i386
bytes. And enabling item 0 removes timer safe points, so the error blocking it
today may disappear with it — a hypothesis to check, not a claim.

The static census could not reach this fact. Producing an executable is this
unit's result, and the first run showed it immediately.

## Verification

| Item | Result |
|---|---|
| x64 link | **succeeds**, zero unresolved engine symbols, ELF 64-bit x86-64 |
| x64 run | **succeeds**, loader / DOS FS / LE placement work, stops at AOT placement, exit 0 |
| i386 `repiu` link | succeeds (9,053,996 bytes) |
| i386 `repiu_core_probe` | 19/19, 0 failures |
| Win32 `repiu_aot_probe` | 41 `_all=true`, 0 `_all=false` |

The new file is inside the x64 gate and does not enter the i386 build, but
`CMakeLists.txt` is shared, so both regressions were checked.

## Not yet verified

- The x64 `repiu` has not been run against **another ROM set**; the observation
  is one run of `pumpit2a`.
- What enabling item 0 produces is a hypothesis and was not measured.
