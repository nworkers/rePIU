# Task 708 작업 로그 — Glide LFB staging surface의 32비트 주소 보장

설계: [20260918-708](../design/20260918-708-glide-lfb-guest-addressable-storage.md) ·
작업 지시: [20260918-708](../work-orders/20260918-708-glide-lfb-guest-addressable-storage.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260918-707](20260918-707-linux-x64-x87-tag-word-conversion.md)

## 수행 결과

게스트가 `grLfbLock`으로 받는 `lfbPtr`이 4 GiB 아래를 가리키게 했습니다.

같은 모양의 문제가 이 저장소에 이미 두 번 있었으므로 — AOT code cache(Task
554)와 shadow selector block(Task 585/586) — 세 번째 사본을 만드는 대신 후보
사다리를 공용 단위 `repiu::runtime::ReserveLowAddressMemory`로 뽑고 기존 두
소비자를 그 위에 다시 얹었습니다. 두 소비자의 공개 API, 후보 목록, 메시지
문자열, 최후수단 정책은 바뀌지 않았습니다.

정책 차이는 호출자에 남겼습니다. code cache는 무힌트 최후수단을 유지하고 4 GiB
초과 결과도 받습니다(자기 거부 메시지가 뒤에 있습니다). shadow selector block과
LFB는 최후수단이 없습니다 — Task 586이 측정했듯 x86-64의 무힌트 `mmap`은 4 GiB
위를 확정적으로 돌려주므로, 그것은 최후수단이 아니라 해제까지 해야 하는
실패입니다.

적합성 판정은 첫 바이트가 아니라 마지막 바이트를 봅니다. shadow selector의 기존
판정을 일반화한 것이고, code cache의 첫 바이트 판정보다 엄격해지는 방향입니다.

`GlideLfbSurface`는 저장소를 **받기만** 합니다(`UseExternalStorage`). 배치는
새 `GlideLfbGuestStorage`가 하고, `repiu::hle`는 지금처럼 플랫폼을 이름 부르지
않습니다. `ThreadContext`가 `shadow_selector_reservation`과 같은 방식으로
소유하고 소멸자에서 풉니다.

`linexe_glide_boundary.cpp`에는 두 `Resize` 호출 앞의 멱등한 설치 호출만
남겼습니다.

## 검증

### probe

* Linux x64 Debug `repiu`, `repiu_core_probe` 빌드 성공
* Linux x64 core probe **28/28 성공**, 실패 0 (신규 group 1개 포함)
* 새 `low_address_reservation` group: 적합성 판정, 후보 응답, 사다리 소진 시
  최후수단 허용/불허, 거부 처리
* `glide_lfb_region` group에 `external_storage` 항목 추가 — 설치 거부 조건,
  들어가지 않는 `Resize` 거부, `pixels()`가 설치된 저장소를 가리키는지,
  쓰기가 그 저장소에 떨어지는지
* 기존 `code_cache_placement` group 회귀 없음: `base=0x20000000 attempt=0
  addressable=1`, 동시 예약 `0x20000000`/`0x28000000`
* Win32 x86 Debug 전체 빌드와 `repiu_core_probe`

### 실제 `pumpit2a` 실행

30초 예산, 감시견 해제, `build/linux_x64_debug/repiu`, WSLg 표시.

수정 전(Task 707 상태):

```text
[repiu-live-debug] grLfbLock granted #1 ... lfbPtr=0xEC02A530 stride=1280 640x480
[repiu-fault] unhandled signal=0xb eip=0x201b4c1f access=0xec02a530 ...
run_exit=139
```

수정 후:

```text
[repiu-live-debug] grLfbLock granted #1 ... lfbPtr=0x1D000000 stride=1280 640x480
[repiu-live-debug] grLfbUnlock #1 non-zero staging bytes=219918/614400
[repiu-shutdown] reason=timeout ... frames=1672 span_ms=10372
run_exit=3
```

`lfbPtr`은 첫 후보 `0x1D000000`이고, 폴트 없이 예산을 채운 뒤 스스로
종료했습니다. `non-zero staging bytes=219918/614400`은 게스트가 그 표면에
실제로 기록했다는 뜻입니다 — 614,400바이트 중 219,918바이트가 0이 아닙니다.
telemetry의 `progress`는 12에서 22로 올라갔습니다.

### 음성 확인에 대해

Task 707처럼 "수정 전 구현에서 새 probe가 실패한다"를 보이지는 못했습니다.
`UseExternalStorage`와 공용 예약 단위는 이 작업에서 처음 생긴 것이라 수정 전
코드에는 호출할 대상 자체가 없습니다. 이 작업의 before/after 증거는 위 실행
비교이고, probe는 새로 만든 계약을 지키는 회귀 감시입니다.

## 남은 것

`grLfbUnlock`이 보고한 `first-texels=0000 0000 0000`은 게스트가 기록한 내용의
왼쪽 위 세 texel이 검다는 것이고, 화면에 옳게 나타나는지는 이 작업이 판정하지
않습니다. 설계에 적어 둔 대로 완료 조건이 아닙니다.

`grLfbLock GrLfbInfo_t caller size=0 (expected 20)` 진단은 이 작업 전후로
동일하며, 별개의 미해결 관찰로 남아 있습니다.

---

## English

Design: [20260918-708](../design/20260918-708-glide-lfb-guest-addressable-storage.md) ·
Work order: [20260918-708](../work-orders/20260918-708-glide-lfb-guest-addressable-storage.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260918-707](20260918-707-linux-x64-x87-tag-word-conversion.md)

### Result

The `lfbPtr` the guest receives from `grLfbLock` now points below 4 GiB.

The same shape of problem already existed twice in this repository — the AOT
code cache (Task 554) and the shadow selector block (Tasks 585/586) — so rather
than add a third copy of the candidate ladder, it moved into a shared
`repiu::runtime::ReserveLowAddressMemory` and both existing consumers were put
back on top of it. Their public APIs, candidate lists, message strings and
last-resort policies are unchanged.

The policy difference stays with the callers. The code cache keeps its unhinted
last resort and accepts a result above 4 GiB, because its own refusal is behind
it. The shadow selector block and the LFB have no last resort: as Task 586
measured, an unhinted `mmap` on x86-64 reliably answers above 4 GiB, so that is
not a fallback but a failure that also has to release.

The fit test looks at the last byte rather than the first. That generalizes the
shadow selector's existing test and is stricter than the code cache's.

`GlideLfbSurface` only **accepts** storage, through `UseExternalStorage`. The
placement is done by the new `GlideLfbGuestStorage`, so `repiu::hle` still names
no operating system. `ThreadContext` owns it the way it already owns
`shadow_selector_reservation` and releases it in its destructor.
`linexe_glide_boundary.cpp` keeps only the idempotent installation call ahead of
each of its two `Resize` sites.

### Verification

**Probes.**

* Linux x64 Debug `repiu` and `repiu_core_probe` built successfully.
* Linux x64 core probe: **28 of 28 passed**, zero failures, including the new
  group.
* The new `low_address_reservation` group covers the fit test, a candidate
  answering, the exhausted ladder with and without a last resort, and the
  refusal paths.
* The `glide_lfb_region` group gained an `external_storage` item: the
  installation refusals, a `Resize` that does not fit being refused, `pixels()`
  pointing at the installed storage, and writes landing in it.
* The existing `code_cache_placement` group did not regress:
  `base=0x20000000 attempt=0 addressable=1`, with concurrent reservations at
  `0x20000000` and `0x28000000`.
* Win32 x86 Debug full build and `repiu_core_probe`.

**Real `pumpit2a` run** — 30-second budget, watchdog off,
`build/linux_x64_debug/repiu`, displayed through WSLg. Before, `grLfbLock`
granted `lfbPtr=0xEC02A530` and the run died with an unhandled SIGSEGV at that
address (`run_exit=139`). After, it grants `lfbPtr=0x1D000000` — the first
candidate — takes no fault, reaches its budget and shuts itself down
(`run_exit=3`). `grLfbUnlock` reports `non-zero staging bytes=219918/614400`,
so the guest really wrote to that surface: 219,918 of 614,400 bytes are
non-zero. The telemetry `progress` counter moved from 12 to 22.

**About the negative check.** Unlike Task 707, this task cannot show the new
probe failing against the pre-fix implementation: `UseExternalStorage` and the
shared reservation unit are new here, so the earlier code has nothing to call.
The before/after evidence for this task is the run comparison above; the probes
are regression guards on the contract this task created.

### What is left

The `first-texels=0000 0000 0000` that `grLfbUnlock` reports says the top-left
three texels of what the guest wrote are black. Whether the content appears
correctly on screen is not judged here, as the design stated.

The `grLfbLock GrLfbInfo_t caller size=0 (expected 20)` diagnostic is unchanged
by this task and remains a separate open observation.
