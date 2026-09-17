# Task 694 — Linux x64 mode16 bare `RETF` HLE

Task 694에서 object 3의 mode16 bare `CB`를 SS-relative 16-bit far-return
HLE로 연결했습니다. 원본 guest bytes는 변경하지 않았습니다.

## 변경 결과

1. `GuestStackReadAccess`와 `ResolveGuestStackReadAccess`를 추가했습니다.
   SS.B=0이면 ESP의 low word를 stack offset으로 사용하고, SS.base를 더한
   선형 주소와 limit/overflow를 검증합니다. frame 소비 후 SP wrap 규칙도
   유지합니다.
2. `ResolveGuestFarReturn16Frame`과 전용 `mode16_far_return` handler를
   추가했습니다. 유효한 `SegCs`가 현재 EIP를 포함하면 우선 사용하고,
   AOT 문맥에 guest CS가 없으면 기존 EIP reverse lookup으로 보완합니다.
   prefix 없는 mode16 `CB`만 4바이트 word IP/CS frame으로 처리하며 target은
   executable selector-relative offset으로만 해석합니다.
3. shared guest dispatcher와 fault HLE chain에 handler를 연결했습니다.
   기존 mode16 `66 CB` 8-byte resolver와 generic 32-bit return은 그대로
   유지합니다.
4. 초기 valid probe에서 CS와 SS의 선형 범위가 겹쳐 reverse lookup이
   의도적으로 모호해지는 사실을 확인했습니다. `SegCs` 우선 규칙과 AOT
   CS 미제공 fallback으로 수정한 뒤 probe가 통과했습니다.

## 검증

WSL Ubuntu 24.04에서 다음 대상을 재빌드했습니다.

```text
make -C build/linux_x64 repiu_core_probe -j1
make -C build/linux_x64 repiu -j1
```

core probe 결과:

```text
[repiu-mode16-far-return] current_cs=0x002C target_ip=0x0020 target_cs=0x0024 target=0x18000220 esp=0x18000900 new_esp=0x18000904
mode16_far_return=1,bad_selector=1,bad_frame=1,mode32_refused=1
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

진단 trace를 끈 기본 동적 `pumpit2a` smoke에서는 object 3 selector binding과
정상적인 timeout cleanup recovery를 확인했습니다.

```text
[loader] Win32 relocated selector binding: selector=0x002C object=3 base=0x01100000 limit=0x00000047
[repiu-shutdown] reason=timeout attempts=39 answered=1 recovered=1 stopped=1 failure=0 eip=0x200633EA gate=0 frames=0 span_ms=0
```

mode16/obj3 진입을 확인하기 위한 `REPIU_AOT_DYNAMIC_CONTAINS=0x01100022`,
`REPIU_AOT_DYNAMIC_TRACE=all`, `REPIU_LINUX_X64_RETURN_TRACE=1` 실행에서는
동적 요청이 `0x010...` 영역에 머물렀고 mode16 handler live trace는 없었습니다.
이 진단 실행은 출력량이 큰 bounded timeout에서 `recovered=0`, `stopped=0`으로
끝났지만 coredump는 재현되지 않았습니다. 따라서 object 3 bare `RETF`,
`0x01100022` LINEXE export 경계, 정상 게임 종료는 미확정입니다.

기존 `execution_trampoline.cpp`의 `g_repiu_active_thread_context` extern
초기화 warning 외 빌드 오류는 없었습니다.

## English

Task 694 connected object 3's mode16 bare `CB` to an SS-relative 16-bit
far-return HLE path. Original guest bytes were not modified.

## Changes

1. Added `GuestStackReadAccess` and `ResolveGuestStackReadAccess`. With SS.B=0,
   the low word of ESP is used as the stack offset; the SS.base-translated
   linear address, descriptor limit, and overflow are validated. The post-frame
   SP wrap rule is preserved.
2. Added `ResolveGuestFarReturn16Frame` and a dedicated `mode16_far_return`
   handler. A valid `SegCs` covering the current EIP is preferred, with the
   existing EIP reverse lookup as a fallback when an AOT context has no guest
   CS. Only prefix-free mode16 `CB` is treated as a four-byte word IP/CS frame,
   and the target is resolved only as an executable selector-relative offset.
3. Connected the handler to the shared guest dispatcher and the fault HLE chain.
   The existing mode16 `66 CB` eight-byte resolver and generic 32-bit return
   path remain unchanged.
4. The initial valid probe exposed the intentional ambiguity of reverse lookup
   when the synthetic CS and SS linear ranges overlapped. Preferring `SegCs`
   and retaining the AOT no-guest-CS fallback fixed that case, and the probe
   then passed.

## Verification

The following targets were rebuilt under WSL Ubuntu 24.04:

```text
make -C build/linux_x64 repiu_core_probe -j1
make -C build/linux_x64 repiu -j1
```

Core-probe result:

```text
[repiu-mode16-far-return] current_cs=0x002C target_ip=0x0020 target_cs=0x0024 target=0x18000220 esp=0x18000900 new_esp=0x18000904
mode16_far_return=1,bad_selector=1,bad_frame=1,mode32_refused=1
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

The baseline dynamic `pumpit2a` smoke with diagnostic tracing disabled confirmed
the object-3 selector binding and successful timeout-cleanup recovery:

```text
[loader] Win32 relocated selector binding: selector=0x002C object=3 base=0x01100000 limit=0x00000047
[repiu-shutdown] reason=timeout attempts=39 answered=1 recovered=1 stopped=1 failure=0 eip=0x200633EA gate=0 frames=0 span_ms=0
```

The diagnostic run using `REPIU_AOT_DYNAMIC_CONTAINS=0x01100022`,
`REPIU_AOT_DYNAMIC_TRACE=all`, and `REPIU_LINUX_X64_RETURN_TRACE=1` remained
in the `0x010...` dynamic-request region and produced no live mode16-handler
trace. That high-volume bounded timeout ended with `recovered=0` and
`stopped=0`, but did not reproduce a coredump. Reaching object 3's bare
`RETF`, the `0x01100022` LINEXE export boundary, and normal game termination
remains unresolved.

There were no build errors apart from the existing
`g_repiu_active_thread_context` extern-initialization warning in
`execution_trampoline.cpp`.
