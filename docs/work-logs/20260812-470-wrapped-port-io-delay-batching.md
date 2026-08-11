# Task 470 작업 로그 — 호출 래퍼형 포트 I/O 지연 루프 batching

## 한국어

### 결과

`pumpitpc`의 200회 JAMMA 지연 루프가 공용 입력 래퍼를 거쳐도 주소 독립적으로
인식됩니다. 동일 Release 바이너리 A/B에서 새 경로는 port I/O를 약 94.2%, privileged
instruction 예외를 약 96.7% 줄였습니다.

| 측정 | OFF | ON |
|---|---:|---:|
| 공통 `AH=08h` 지점 도달 시간 | 8.29초 | 6.86초 |
| JAMMA scan | 233,280 | 3,527 |
| 전체 port I/O | 244,970 | 14,110 |
| `0xC0000096` 예외 | 236,962 | 7,936 |
| batch / 생략 반복 | 0 / 0 | 783 / 155,034 |

최대 생략 반복은 198이고 마지막 일치 loop body와 limit는 각각 relocation 기준
`0x03016CF7`, 200이었습니다. 이는 정적 분석한 `mov eax,0x2A8; call; inc edx;
cmp edx,200; jl`과 일치합니다.

### 구현

- 입력 래퍼의 레지스터 보존, port 전달, EAX zeroing, `IN`, `RET`을 검증합니다.
- guest stack 반환 주소와 앞선 `call rel32` target을 검증합니다.
- caller loop body가 EAX를 즉시 port 상수로 덮어 입력 결과를 폐기하는지 확인합니다.
- stack에 저장된 counter만 전진시키며 guest가 마지막 반복과 flags 계산을 수행합니다.
- wrapped candidate/batch/마지막 return을 별도 통계로 남깁니다.

target profile, 실행 파일 이름, 고정 code address는 matcher 조건에 포함되지 않습니다.

### 검증

- Win32 x86 Debug 및 Release 전체 빌드 성공
- `repiu_aot_probe` 성공: `valid=true`, `cache_valid=true`,
  `port_io_dispatch_specific=true`, `selector_guard_all=true`, `coherence_all=true`
- 실제 `pumpitpc` ON/OFF 실행 모두 같은 기존 `unsupported DOS INT 21h AH=0x8`에
  도달했습니다. 따라서 이 지점은 새 batching 회귀가 아니며 별도 HLE 작업으로 남습니다.

## English

### Result

The 200-read `pumpitpc` JAMMA delay loop is now recognized through its shared input wrapper without
using addresses. In a same-binary Release A/B, the path reduced total port I/O by about 94.2% and
privileged-instruction exceptions by about 96.7%.

| Measurement | OFF | ON |
|---|---:|---:|
| Time to common `AH=08h` endpoint | 8.29 s | 6.86 s |
| JAMMA scans | 233,280 | 3,527 |
| Total port I/O | 244,970 | 14,110 |
| `0xC0000096` exceptions | 236,962 | 7,936 |
| Batches / skipped iterations | 0 / 0 | 783 / 155,034 |

The maximum skip was 198, and the last matched body and limit were relocated `0x03016CF7` and
200, matching the statically inspected `mov eax,0x2A8; call; inc edx; cmp edx,200; jl` loop.

### Implementation

The matcher proves wrapper register preservation, port transfer, EAX zeroing, `IN`, and `RET`; the
guest-stack return address and preceding `call rel32` target; and the caller body that overwrites
EAX with the port constant before every skipped input. It advances only the saved counter, leaving
the final iteration and flags to the guest. Separate wrapped-candidate, batch, and last-return
statistics make activation visible. No target profile, executable name, or fixed code address is
part of the match.

### Verification

The full Win32 x86 Debug and Release builds succeeded. `repiu_aot_probe` reported `valid=true`,
`cache_valid=true`, `port_io_dispatch_specific=true`, `selector_guard_all=true`, and
`coherence_all=true`. Both live ON and OFF runs reached the same pre-existing
`unsupported DOS INT 21h AH=0x8`, leaving that endpoint as a separate HLE task rather than a
batching regression.
