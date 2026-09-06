# Task 618 작업 로그: Linux x64 direct-call 게스트 스택 provenance 추적

## 한국어

### 수행 내용

* Linux x64 dispatch frame에 64개 고정 record ring을 추가했습니다.
* `REPIU_LINUX_X64_STACK_TRACE=1`일 때 AOT direct `CALL` emission 직후에
  host flags를 보존하는 기록 sequence를 삽입했습니다.
* zero return-frame trace가 `guest_esp - 4` consumed slot과 일치하는 기록을
  출력하도록 했습니다.

### 검증

빌드:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

초기 실행에서 image decode 검증이 실패했습니다. 원인은 trace sequence의
86바이트를 86개 명령으로 보고한 계수 오류였고, 실제 20개 명령으로 수정한
뒤 `repiu`가 정상 빌드됐습니다.

core probe:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

runtime:

```text
REPIU_LINUX_X64_STACK_TRACE=1 \
REPIU_LINUX_X64_RETURN_FRAME_TRACE=1 \
REPIU_LINUX_X64_RETURN_TRACE=1 \
timeout 60 ./build/linux_x64_repiu/repiu pumpit2a
```

결과는 다음과 같습니다.

* zero `RET` site: `0x010F101D`
* consumed slot: `0x0158CC48`
* direct-call match: 3건
* sites/values: `0x010F729A -> 0x010F729F`,
  `0x010F72BC -> 0x010F72C1`, `0x010F7304 -> 0x010F7309`
* 세 record 모두 기록 후 ESP와 값이 `0x0158CC48` 및 올바른 nonzero
  fallthrough와 일치
* 실행은 기존 fail-closed x64 return `INT3`/`UD2`에서 종료

### 결론 및 다음 단계

direct `CALL`의 반환 주소 기록은 정상입니다. zero는 마지막 direct `CALL`
이후의 다른 stack write 또는 stack-frame 계약 문제에서 발생합니다. 이번
작업에서는 해당 값을 보정하지 않았습니다. 다음 작업은 일반 guest stack
store/push 또는 HLE 경계 중 어느 경로가 `0x0158CC48`을 마지막으로 덮는지
추적하는 것입니다.

## English

### Work performed

* Added a fixed 64-record ring to the Linux x64 dispatch frame.
* When `REPIU_LINUX_X64_STACK_TRACE=1`, inserted a host-flags-preserving
  record sequence after AOT direct-call emission.
* Made the zero return-frame trace print records matching `guest_esp - 4`, the
  consumed slot.

### Verification

Build:

```text
cmake --build build/linux_x64_repiu --target repiu_core_probe repiu -j 2
```

The first run failed image decode verification because the 86-byte trace
sequence was reported as 86 instructions. Reporting its actual 20 instructions
fixed the issue and the Linux x64 executable built normally.

Core probe:

```text
core_probe_total=24
core_probe_failures=0
core_probe_all=true
```

Runtime:

```text
REPIU_LINUX_X64_STACK_TRACE=1 \
REPIU_LINUX_X64_RETURN_FRAME_TRACE=1 \
REPIU_LINUX_X64_RETURN_TRACE=1 \
timeout 60 ./build/linux_x64_repiu/repiu pumpit2a
```

The result was:

* zero `RET` site: `0x010F101D`
* consumed slot: `0x0158CC48`
* direct-call matches: 3
* sites/values: `0x010F729A -> 0x010F729F`,
  `0x010F72BC -> 0x010F72C1`, `0x010F7304 -> 0x010F7309`
* all three records had post-write ESP `0x0158CC48` and the correct nonzero
  fallthrough value
* execution still stopped at the existing fail-closed x64 return `INT3`/`UD2`

### Conclusion and next step

The direct-call return-address writes are correct. The zero originates from a
different stack write or stack-frame contract issue after the last direct CALL
and before RET. This task did not repair the value. The next task should trace
which ordinary guest stack store/push or HLE boundary last overwrites
`0x0158CC48`.
