# Task 522 작업 로그 — 게스트 소유 INT3

설계: [20260829-522](../design/20260829-522-guest-owned-breakpoint.md) ·
작업 지시: [20260829-522](../work-orders/20260829-522-guest-owned-breakpoint.md)

## 한 일

* `src/engine/exception/guest_owned_breakpoint.{h,cpp}` 신규.
* `execution_trampoline.cpp` 폴트 체인 끝에 연결.
* `AppendConsoleOutput`에 게스트 표준출력 즉시 반향 (`REPIU_DOS_INT_TRACE`).
* `RecordHandledDosInterrupt`에 DOS/DPMI 호출 추적 (`REPIU_DOS_INT_TRACE`).
* `thread_context.h`에 `guest_owned_breakpoint_count`.

## 측정

실제 Ubuntu (VMware, 커널 7.0.0-30-generic), `pumpit1`, 25초.

| | 전 | 후 |
|---|---:|---:|
| `0x010F3438` 단일 스텝 샘플 | 814,138 | — |
| `[repiu-guest-int3]` | (핸들러 없음) | **1** |
| 게스트 진단 문장 | 삼켜짐 | **인쇄됨** |

```
[repiu-guest-int3] #1 eip=0x010F3438 eax=0x00000000 edx=0x011A623C
[repiu-guest-out] Fatal error: unable to initialize DLL loader.
```

`edx=0x011A623C` = DS 베이스 `0x01110000` + `0x9623C`. 세 fatal 분기 중 **첫 번째**
(`eax==0`)임을 이 값이 확정했습니다.

## 고친 실수

`0xCC` 검사를 카운터와 로그 **뒤**에 두어, INT3가 아닌 폴트도 세고 찍은 다음 거절하고
있었습니다. 검사를 앞으로 옮겼습니다.

## 판단 실수 — 이미 문서화된 것을 다시 유도했습니다

`docs/analysis/dll-loader-int21-ff00.md`에 이 실패의 주소와 분기가 이미 적혀 있었습니다.
`AGENTS.md`는 설계 전에 `docs/analysis/`를 읽으라고 하는데 하지 않았습니다.

다만 그 문서의 **원인 서술은 낡아 있었습니다** — "`AX=FF00h` HLE가 임시로 `AL=0`을 반환한다"고
되어 있으나, 실제 핸들러는 `kDos4gwIdentificationAxResult`와 `kDos4gwClientDataSelector`를
이미 구현하고 있습니다. 낡은 문서를 근거로 원인을 한 번 잘못 말했고 [Task 523]에서
정정했습니다. **문서를 먼저 읽되, 코드로 확인해야 합니다.**

## 범위

원인 수정이 아니라 진단 가능성 회복입니다. 실제 원인은
[Task 523](20260829-523-sibling-asset-case.md).

## 검증

* Linux i386 (실제 Ubuntu): 빌드 통과, 위 측정.
* Windows x86 debug: 빌드 통과.
