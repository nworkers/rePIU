# 작업 로그 20260905-595 — Linux x64 INT 31h 미지원 함수 HLE

## 결과

Linux x64 실행에서 확인된 `INT 31h AX=1E7F`를 원본 게스트 코드가
실행되지 않도록 DPMI HLE의 미지원 함수 결과로 처리했습니다. 기존
fall-through가 raw `INT 31h`를 호스트 long mode에서 실행하던 경로를
제거하고, `AX=8001h`, CF set, EIP `+2`를 반환하도록 했습니다.

## 확인

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
```

```text
core_probe_total=20
core_probe_failures=0
core_probe_all=true
```

실행 로그에서 다음을 확인했습니다.

```text
[repiu-dos-int] #3 int=31 ax=1E7F
[repiu-watch] event=step guest=0x010F010C ...
[repiu-watch] event=fault guest=0x010F022C ...
```

`0x010F010C`의 raw `INT 31h` SIGSEGV는 사라졌고, `AX=1E7F` 요청은 HLE
처리 후 게스트 `0x010F022C` frontier로 진행했습니다. 이 frontier에서
게스트 소유 `INT3` 재진입 문제가 새로 드러났으며 Task 596으로 이어졌습니다.

## 판단

미지원 DPMI 함수에 대한 일반 오류 반환은 원본 게임 로직을 재작성하지
않고 OS/DPMI 경계만 HLE로 대체하는 프로젝트 원칙에 부합합니다. 이후
확인된 게스트 `INT3`는 별도 breakpoint ownership 문제로 분리했습니다.

## English

# Work log 20260905-595 — Linux x64 INT 31h unsupported-function HLE

## Result

The Linux x64 run identified `INT 31h AX=1E7F`. It is now handled as an
unsupported DPMI function by the HLE layer, so the raw guest `INT 31h` is not
executed in host long mode. The handler returns `AX=8001h`, sets CF, and
advances EIP by two bytes.

## Verification

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
```

```text
core_probe_total=20
core_probe_failures=0
core_probe_all=true
```

The runtime log confirmed:

```text
[repiu-dos-int] #3 int=31 ax=1E7F
[repiu-watch] event=step guest=0x010F010C ...
[repiu-watch] event=fault guest=0x010F022C ...
```

The raw `INT 31h` SIGSEGV at `0x010F010C` disappeared, and the `AX=1E7F`
request advanced through HLE to guest frontier `0x010F022C`. A guest-owned
`INT3` reentry issue was then exposed at that frontier and handled by Task 596.

## Assessment

Returning the generic unsupported-DPMI error replaces only the DOS/DPMI
boundary and preserves the original guest logic. The later guest `INT3` was
tracked separately as a breakpoint-ownership issue.
