# Linux x64 ESP 메모리 fault 조사 작업 지시서

## 한국어

### 작업 목표

Task 606 이후 새로 관찰된 `0x010F1E0F` fault가 AOT 번역 누락인지,
legacy fallback의 원시 실행인지, 또는 fault HLE의 주소 계산 문제인지
분리하고 첫 번째 원인에 해당하는 공용 경로를 수정합니다.

### 작업 범위

1. `HandleAotReentry`의 legacy fallback에 제한적인 opt-in 진단을 추가합니다.
2. `80 3C 24 00`에 대한 classifier/lowering probe를 추가하거나 보강합니다.
3. 진단 결과에 따라 AOT lowerer, dynamic translation 연결, 또는 guest fault
   handler 중 최소 한 곳만 수정합니다.
4. Linux x64 debug build, core probe, 실제 guest 실행으로 검증합니다.
5. analysis 문서와 작업 로그를 갱신하고 task branch에 커밋합니다.

### 제외 범위

* 특정 guest EIP만 통과시키는 주소 특례
* 게임 로직의 C++ 재구현
* DOSBox 또는 전체 CPU emulator 도입
* fault를 무시하거나 SIGSEGV를 무조건 resume하는 처리

### 완료 조건

원인 분류에 필요한 로그가 재현되고, 공용 x64 경로의 수정 및 probe가
성공하며, runtime 재실행 결과와 남은 frontier가 작업 로그에 기록되어야
합니다.

## English

### Objective

Separate whether the new `0x010F1E0F` fault observed after Task 606 is an AOT
translation omission, raw execution through legacy fallback, or an address
calculation issue in the fault HLE path, then fix the corresponding shared
path.

### Scope

1. Add bounded opt-in diagnostics to `HandleAotReentry`'s legacy fallback.
2. Add or strengthen classifier/lowering coverage for `80 3C 24 00`.
3. Based on evidence, extend the shared traced compare fault path to
   `CMP r/m8, imm8`.
4. Verify with the Linux x64 debug build, core probes, and the real guest.
5. Update analysis and work-log documents and commit the task branch.

### Out of scope

* An address-specific exception for a guest EIP
* Reimplementing game logic in C++
* Introducing DOSBox or a full CPU emulator
* Ignoring the fault or unconditionally resuming SIGSEGV

### Done criteria

The diagnostic reproduces the cause classification, the shared x64 path and
probe pass, and the runtime result plus any remaining frontier are recorded
in the work log.

### Current classification

The runtime confirms `0x010F920C` is an unmapped legacy-fallback entry and the
lowerer is correct. The implementation target is therefore guest-ESP-based
8-bit immediate-compare fault handling.

### Additional Task 607 scope

후속 runtime frontier는 x64 HLE 종료 경로 오류입니다. DOS 종료 뒤 signal-resume
context가 `RecoverGuestStackException`의 의도된 `ud2`에 도달했습니다.
원본 cache-entry ABI를 유지하는 x64 종료 경로를 추가하고, Linux signal resume에는
전체 host RIP를 기록하며, i386 복구 경로는 변경하지 않습니다. DOS 종료가
SIGSEGV/SIGILL 없이 guest thread에서 반환되는지 검증합니다.

### Additional Task 607 scope (English)

The subsequent runtime frontier is an x64 HLE exit failure: after DOS
termination, the signal-resume context reaches the deliberate `ud2` in
`RecoverGuestStackException`. Add an x64 exit mechanism that preserves the
original cache-entry ABI, writes a full host RIP for Linux signal resume, and
leaves the i386 recovery path unchanged. Verify that DOS termination returns
from the guest thread without SIGSEGV or SIGILL.
