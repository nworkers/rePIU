# JAMMA 시스템 키 매핑 수정 작업 로그
# JAMMA System Key Mapping Correction Work Log

* 작업 번호 / Task: 361
* 작성일 / Date: 2026-07-30
* 브랜치 / Branch: `fix/jamma-system-key-mapping`

## 1. 결과
## 1. Result

Win32 JAMMA 시스템 포트 `0x02A9`의 키를 요청한 네 기능으로 분리했다.
기존 F1=`0x80` TEST/CLEAR 혼합 매핑을 제거하고 TEST와 CLEAR를 각각
F1=`0x02`, F3=`0x80`으로 분리했다. F2 SERVICE=`0x40`과 F5
COIN1=`0x04`도 독립된 진단 이름으로 유지했다.

Separated the Win32 JAMMA system-port `0x02A9` keys into the four requested
functions. Removed the combined F1=`0x80` TEST/CLEAR mapping and separated TEST
and CLEAR into F1=`0x02` and F3=`0x80`. F2 SERVICE=`0x40` and F5
COIN1=`0x04` retain independent diagnostic names.

## 2. 변경
## 2. Changes

* `src/platform/win32/io/port_io_emulator.cpp`
  * `kJammaBitsSystem`을 TEST, COIN1, SERVICE, CLEAR로 수정했다.
  * `ReadJammaPort8`에 F1, F2, F3, F5 매핑을 적용했다.
* `docs/analysis/piu-io-port-specification.md`
  * 확인된 MAME 비트와 rePIU SERVICE 호환 정책을 구분해 기록했다.
* Task 361 설계 문서와 작업 지시 문서를 추가했다.

* `src/platform/win32/io/port_io_emulator.cpp`
  * Corrected `kJammaBitsSystem` to TEST, COIN1, SERVICE, and CLEAR.
  * Applied the F1, F2, F3, and F5 mappings in `ReadJammaPort8`.
* `docs/analysis/piu-io-port-specification.md`
  * Documented confirmed MAME bits separately from rePIU's SERVICE
    compatibility policy.
* Added the Task 361 design and work-order documents.

## 3. 검증
## 3. Verification

### 빌드 / Build

`cmake --build build\win32_x86_debug --config Debug --target
repiu_loader_win32 repiu_supervisor_win32`가 오류 없이 성공했다.

`cmake --build build\win32_x86_debug --config Debug --target
repiu_loader_win32 repiu_supervisor_win32` completed without errors.

### 실제 게스트 입력 / Live guest input

`aot-dbt` 백엔드로 `pumpit1`을 실행하고 `0x02A9` 폴링 시작을 기다린 뒤
Win32 합성 키를 순서대로 주입했다. 모든 press/release가 예상한 active-low
값으로 관측됐다.

Ran `pumpit1` with the `aot-dbt` backend, waited for `0x02A9` polling to start,
and injected the Win32 keys in sequence. Every press/release produced the
expected active-low value.

```text
[repiu-input] TEST    PRESSED  port=0x02A9 value=0xFD
[repiu-input] TEST    released port=0x02A9 value=0xFF
[repiu-input] SERVICE PRESSED  port=0x02A9 value=0xBF
[repiu-input] SERVICE released port=0x02A9 value=0xFF
[repiu-input] CLEAR   PRESSED  port=0x02A9 value=0x7F
[repiu-input] CLEAR   released port=0x02A9 value=0xFF
[repiu-input] COIN1   PRESSED  port=0x02A9 value=0xFB
[repiu-input] COIN1   released port=0x02A9 value=0xFF
```

검증 로그:
`build/task361_input_20260730_023548/stderr.log`

Verification log:
`build/task361_input_20260730_023548/stderr.log`

F1 단독 검증에서는 TEST press 뒤 게스트 진행 지표가 `8149`에서 `8159`로
증가하고 실행 경로가 전환되어, 이전 CLEAR 비트가 아니라 TEST 처리 분기가
실행됨을 확인했다.

In the F1-only verification, the guest progress marker advanced from `8149` to
`8159` after TEST was pressed and execution changed paths, confirming that the
TEST branch executes instead of the former CLEAR bit.

## 4. 비고
## 4. Notes

포트 주소, 다중 너비 IN 조합, 실시간 폴링, EIP 전진 및 원본 게스트 코드는
변경하지 않았다.

Port addressing, multi-width IN composition, live polling, EIP advancement,
and original guest code were not changed.
