# Task 606: word stack 반환주소 손상 수정 / Word-stack return-address repair

## 한국어

### 원인과 수정

Task 605는 AOT map과 디코드 가능성으로 의도적 진입을 확정했으나,
trace 범위를 넓히자 그 전에 반환주소가 손상되는 것이 드러났다.

```text
before:
#16 eip=0x010F839E esp=0x0158CC5C stack=0x010F4B7E eax=0x00001E7F
#17 eip=0x010F83C2 esp=0x0158CC5C stack=0x010F0103 eax=0x00004B7E
#18 eip=0x010F0107 esp=0x0158CC60 eax=0x00001E7F
```

원본 `0x010F839E`는 `66 50` PUSH AX, `0x010F83C2`는 `66 58` POP AX이다.
그 사이의 `FLDCW [ESP]`, `XCHG AX,[ESP]`는 word stack 보존에 의존한다.
Task 559가 이 PUSH/POP lowering을 지원하지 않았으므로 기존 stack sequence에
2바이트 이동과 16비트 MOV를 추가했다. R15D가 guest ESP이며 상위 register 비트와
flags를 유지한다. SP도 감소 전 저장/증가 후 하위 word 반영을 처리한다.

### 검증

```bash
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
./build/linux_x64_debug/repiu_core_probe
REPIU_DOS_INT_TRACE=1 REPIU_DPMI_1E7F_TRACE=1 timeout -k 1s 12s ./build/linux_x64_debug/repiu pumpit2a
```

빌드는 exit 0으로 완료했다. core probe 23개가 모두 통과했다.
register probe에 word 저장 폭, 상위 EAX, ESP ±2, ZF, 인접 반환주소,
PUSH SP/POP SP 검증을 추가했고 `linux_x64_guest_register_all=true`였다.
추가 실행에서 `REPIU_LINUX_X64_RETURN_TRACE=1`로 아래 복귀를 확인했다.

```text
core_probe_total=23
core_probe_failures=0
core_probe_all=true
[repiu-x64-return] result=resolved source=0x010F4B7E cache=0x2004FD68
[repiu-x64-return] result=resolved source=0x010F4BBA cache=0x2004FCD8
```

기본 실행은 진단용 성공 옵션 없이 다음 지점으로 진행했다.

```text
[repiu-fault] unhandled signal=0xb rip=0x10f1e0f eip=0x10f1e0f
access=0xc7ffec0c bytes=80 3c 24 00 75 0b 89 f0 ... esp=0x158cc68
```

다른 실행에서는 access가 `0x4b7fdc0c`였다. 두 실행은 같은 guest 주소에서
정지했다. 새 오류의 정확한 host/guest stack 전달 원인은 아직 미확정이다.
게임 화면 도달을 달성한 것은 아니다. Windows 빌드는 이번에 수행하지 않았으며,
변경은 long-mode lowering과 Linux x64 실행 probe에 국한된다.
Task 605 설계·로그·analysis의 사설 ABI 결론을 정정하고 누적 문서를 갱신했다.

## English

### Cause and change

Task 605 inferred intentional entry from AOT registration and decodability.
The expanded trace above instead exposes earlier return-address corruption.
Original `0x010F839E` is `66 50` PUSH AX and `0x010F83C2` is `66 58` POP AX.
The intervening `FLDCW [ESP]` and `XCHG AX,[ESP]` require word-stack handling.
Task 559 did not lower these forms. The existing sequence now uses two-byte ESP
updates and word MOV, preserving upper register bits and flags. SP captures its
pre-decrement value and updates the low word after increment.

### Verification

The commands above built successfully (exit 0); all 23 core probes passed.
The register probe checks word-store width, upper EAX, ESP ±2, ZF, adjacent return
data, and PUSH SP/POP SP; `linux_x64_guest_register_all=true`.
A further run with `REPIU_LINUX_X64_RETURN_TRACE=1` resolves the original
`0x010F4B7E` return and then `0x010F4BBA`.
The default run no longer calls `1E7Fh` and reaches a new SIGSEGV at `0x010F1E0F`,
bytes `80 3C 24 00`, guest ESP `0x0158CC68`. Access addresses differ between runs
(`0xc7ffec0c`, `0x4b7fdc0c`). The new host/guest stack cause remains unresolved;
gameplay/display completion is not claimed. No Windows build was run for this
long-mode lowering and Linux x64 probe change.
Task 605 design/log/analysis now carry corrections; cumulative documents were updated.
