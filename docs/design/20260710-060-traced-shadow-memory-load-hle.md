# Traced shadow memory load HLE 설계

## 배경

`66 C7` word store 처리 뒤 `piu_1st`는 `0x0201DF24`의 `8B 50 18`에서 중단된다.

이 명령은 `mov edx, dword ptr [eax+0x18]`이며, `EAX=0x025E700C`이므로 source는 runtime arena 밖의 `0x025E7024`이다. 해당 주소는 직전 흐름에서 out-of-arena store로 초기화된 metadata/value 영역이다.

기존에는 마지막 out-of-arena store 한 개만 기억했기 때문에 여러 필드를 초기화한 뒤 과거 필드를 다시 읽는 흐름을 처리할 수 없다.

## 정책

* out-of-arena store를 byte-addressed shadow memory에 기록한다.
* `C7`, `66 C7`, `D9 FST/FSTP`의 skipped store는 shadow memory에도 반영한다.
* `8B /r` 중 SIB 없는 32-bit ModR/M memory source를 처리한다.
* source가 runtime arena 안이면 실제 dword read를 수행한다.
* source가 runtime arena 밖이면 shadow memory에 4바이트가 모두 있을 때만 register load를 처리한다.
* register destination만 지원하고, SIB addressing과 displacement-only addressing은 이번 범위에서 제외한다.

# Traced Shadow Memory Load HLE Design

## Background

After `66 C7` word-store handling, `piu_1st` stops at `8B 50 18` at `0x0201DF24`.

This instruction is `mov edx, dword ptr [eax+0x18]`. With `EAX=0x025E700C`, the source is out-of-arena address `0x025E7024`. That address belongs to the metadata/value area initialized through previous out-of-arena stores.

The previous implementation remembered only the last out-of-arena store, so it could not handle a later read from an older initialized field.

## Policy

* Record out-of-arena stores in byte-addressed shadow memory.
* Reflect skipped stores from `C7`, `66 C7`, and `D9 FST/FSTP` into shadow memory.
* Handle `8B /r` 32-bit ModR/M memory sources without SIB.
* Read the actual dword when the source is inside the runtime arena.
* When the source is outside the runtime arena, handle the register load only if all 4 bytes exist in shadow memory.
* Support register destinations only; SIB addressing and displacement-only addressing are out of scope.
