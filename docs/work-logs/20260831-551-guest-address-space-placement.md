# 20260831-551 guest 주소 공간 배치 측정 작업 로그

## 한국어

### 결과

Task 546 결정 4의 "하위 4 GiB 배치"를 측정으로 정리했습니다. 결정 4가 요구한 두
갈래의 답이 실제로 다릅니다.

**(a) guest memory는 하위 4 GiB에 놓을 수 있습니다 — 측정된 사실입니다.**

x86-64 Linux가 PIU 프로파일의 arena를 요청한 base에 **정확히** 내줍니다.

```text
guest_arena base=0x10000 size=0x85d7000 placed=true host_error=0 overlaps_own_image=0
```

134 MB를 `0x00010000`에 `MAP_FIXED_NOREPLACE`로 예약해 성공합니다. guest의 relocation이
이미 그 주소를 guest memory 안에 써 넣었으므로 이것은 선호가 아니라 요구사항이었고,
x64에서 거절당했다면 port가 거기서 끝났을 것입니다. 거절당하지 않습니다.

**(b) host pointer는 부분적으로만 하위 4 GiB입니다.**

| 항목 | Linux x64 | 정하는 주체 |
|---|---|---|
| engine 자신의 code | 4 GiB 아래 | Task 503의 `-no-pie -Wl,-Ttext-segment=0x40000000` |
| heap | 4 GiB 아래 | non-PIE image를 따라감 |
| stack | **4 GiB 위** | 커널 |
| 실행 가능한 매핑 (libc 등) | **4 GiB 위**, 최고 `0x788914de2000` | `ld.so`, 그리고 런타임 `dlopen` |

engine 이미지가 낮은 것은 우연이 아닙니다. Task 503이 guest 재배치 범위 밖에 두려고
`0x400000`이 아닌 `0x40000000`을 고른 결과입니다. 그러나 stack과 shared library는
build가 정할 수 있는 것이 아니므로, **그 둘을 담을 수 있는 host pointer는
`uintptr_t`여야 합니다.** 결정 4가 제거하라고 한 "우연한 가정"이 바로 이 부분입니다.

### 측정으로 드러난 위험: `mmap_min_addr` 여유가 0

```text
guest_address_space_mmap_min_addr=65536,lowest_guest_base=65536,headroom=0
```

PIU 프로파일의 base `0x00010000`이 커널 기본 floor와 **정확히 같은 값**입니다. 여유가
한 바이트도 없으므로, `vm.mmap_min_addr`을 조금이라도 높인 배포판·컨테이너·hardened
설정에서는 guest를 배치할 수 없습니다. x64만의 문제가 아니라 i386에도 똑같이
해당하며, 지금까지 드러나지 않은 것은 기본값이 마침 같았기 때문입니다.

### Win32에서 배치가 실패하는 이유

Win32 core probe는 두 요청 모두 `host_error=487`(`ERROR_INVALID_ADDRESS`)로 실패합니다.
이것은 Windows의 한계가 아니라 **묻고 있는 바이너리 자신의 문제**입니다.

```text
guest_arena base=0x10000 size=0x85d7000 placed=false host_error=487 overlaps_own_image=1
```

평범한 MSVC 실행 파일은 `0x400000`에 놓이고, 그 주소는 arena 범위 `0x10000`–`0x85E7000`
안입니다. 실제 Win32 loader host는 `/BASE:0x10000000`(268 MB)로 링크되어 arena 위쪽에
있으므로 충돌하지 않습니다. probe가 `overlaps_own_image`를 함께 보고하는 이유가
이것입니다 — 이 줄이 없으면 `placed=false`가 "Windows는 guest를 배치할 수 없다"로
읽힙니다.

### 계약과 측정을 나눈 이유

probe는 **계약**만 pass/fail로 판정합니다: guest base에 대한 예약은 정확히 그
주소로 오거나 아예 오지 않아야 하고, 다른 주소가 조용히 받아들여지면 안 됩니다.
정확한 배치의 성공 여부는 host마다 다른 **측정값**이라 값으로만 남깁니다.

이 구분이 바로 위 Win32 사례에서 값을 했습니다. 배치 실패를 probe 실패로 만들었다면
Win32 core probe 전체가 빨간색이 됐을 것이고, 그것은 사실이 아닌 신호입니다.

### 검증

| Host | arena 배치 | `core_probe_all` |
|---|---|---|
| Linux x64 Debug | `placed=true` | true, 16/16, skipped 2 |
| Linux i386 Release | `placed=true` | true, 17/17 |
| Win32 x86 Debug | `placed=false` (own image overlap) | true, 17/17 |

## English

### Result

Task 546's decision 4 -- the "place below 4 GiB" item -- is now settled by measurement,
and the two halves it asks to be separated really do have different answers.

**(a) Guest memory can live below 4 GiB. That is measured, not assumed.**

x86-64 Linux gives the PIU profile's arena the exact base it asks for:

```text
guest_arena base=0x10000 size=0x85d7000 placed=true host_error=0 overlaps_own_image=0
```

134 MB reserved at `0x00010000` with `MAP_FIXED_NOREPLACE`, successfully. The guest's
relocations have already written that address into guest memory, so this was a
requirement rather than a preference, and a refusal on x64 would have ended the port
there. It is not refused.

**(b) Host pointers are only partly below 4 GiB.**

| Item | Linux x64 | Decided by |
|---|---|---|
| The engine's own code | below 4 GiB | Task 503's `-no-pie -Wl,-Ttext-segment=0x40000000` |
| Heap | below 4 GiB | follows the non-PIE image |
| Stack | **above 4 GiB** | the kernel |
| Executable mappings (libc and friends) | **above 4 GiB**, highest `0x788914de2000` | `ld.so`, and `dlopen` at runtime |

The engine image being low is not an accident: Task 503 chose `0x40000000` rather than
`0x400000` to stay clear of the guest's relocation range. But the stack and the shared
libraries are not the build's to place, so **any host pointer that can hold one of those
must be `uintptr_t`.** That is precisely the "accidental assumption" decision 4 asks to
be removed.

### A risk the measurement exposed: zero `mmap_min_addr` headroom

```text
guest_address_space_mmap_min_addr=65536,lowest_guest_base=65536,headroom=0
```

The PIU profiles' base `0x00010000` is **exactly** the kernel's default floor. There is
not one byte of margin, so any distribution, container, or hardened configuration that
raises `vm.mmap_min_addr` at all cannot place the guest. This is not specific to x64 --
i386 has the same exposure, and it has stayed invisible only because the default happens
to be the same number.

### Why placement fails on Win32

The Win32 core probe fails both requests with `host_error=487`
(`ERROR_INVALID_ADDRESS`). That is not a limit of Windows but a property of **the binary
doing the asking**:

```text
guest_arena base=0x10000 size=0x85d7000 placed=false host_error=487 overlaps_own_image=1
```

An ordinary MSVC executable is based at `0x400000`, which is inside the arena range
`0x10000`–`0x85E7000`. The real Win32 loader host links at `/BASE:0x10000000` (268 MB),
above the arena, and does not collide. Reporting `overlaps_own_image` alongside the
refusal is what keeps `placed=false` from reading as "Windows cannot place the guest".

### Why the contract and the measurement are separated

The probe judges only the **contract**: a reservation at the guest base must land exactly
there or not count, and a different address must never be quietly accepted. Whether exact
placement succeeds is a **measurement** that differs by host, so it is reported as a value.

That separation earned its keep on the Win32 case above. Had placement failure been a
probe failure, the entire Win32 core probe would have gone red, and that would have been
a false signal.

### Verification

| Host | Arena placement | `core_probe_all` |
|---|---|---|
| Linux x64 Debug | `placed=true` | true, 16 of 16, 2 skipped |
| Linux i386 Release | `placed=true` | true, 17 of 17 |
| Win32 x86 Debug | `placed=false` (own image overlap) | true, 17 of 17 |
