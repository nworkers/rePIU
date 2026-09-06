# 작업 지시서 20260906-609 — Linux x64 LINEXE direct-dispatch capability

## 목표

Linux x64에서 제공되지 않는 Glide direct-dispatch thunk 때문에 LINEXE
환경 전체가 비활성화되는 문제를 수정합니다. direct patch가 불가능한
경우에도 기존 trap/HLE Glide gate를 기록하여 원본 guest 초기화가 계속
진행되도록 합니다.

## 작업 순서

1. `GetGlideGateDirectDispatchThunkAddress()`의 host capability를
   `RunExecutionThread`의 direct-dispatch 요청과 분리합니다.
2. capability가 없는 경우 `direct_glide_dispatch=false`로 설정하고
   `PatchGlideGatePlanForDirectDispatch`를 호출하지 않습니다.
3. base Glide gate image 기록, descriptor 등록, memory protection이 기존
   방식으로 수행되는지 확인합니다.
4. loader 상태 로그와 `REPIU_LINEXE_INIT_TRACE`가 요청/실제 capability를
   혼동하지 않도록 갱신합니다.
5. core probe, Linux x64 기본 실행, `CON` 회귀 경로를 검증합니다.
6. 다음 DOS/DPMI frontier를 확인하고 설계·분석·작업 로그를 갱신합니다.

## 완료 조건

* Linux x64 기본 실행에서 `direct=0`, `glide_fits=1`, `active=1`입니다.
* `AX=FF00h`가 `EAX=0000FFFFh`, `GS=0x20`으로 반환됩니다.
* `CON` handle `0x0005` open/write가 회귀하지 않습니다.
* 기존 direct-dispatch capability가 있는 경로의 patch/verification 동작은
  유지됩니다.
* core probe가 `24/24` 성공합니다.
* 실제 실행 결과와 다음 frontier가 작업 로그에 기록됩니다.

## 금지 사항

* 64비트 host pointer를 32비트 guest image에 강제 삽입하지 않습니다.
* 특정 guest EIP 예외나 원본 binary patch를 추가하지 않습니다.
* direct dispatch 불가를 이유로 전체 LINEXE 환경을 실패 처리하지 않습니다.

## English

### Objective

Fix the Linux x64 case where an unavailable Glide direct-dispatch thunk
disables the entire LINEXE environment. When the optional direct patch cannot
be installed, write the existing trap/HLE Glide gate so original guest
initialization can continue.

### Sequence

1. Separate `GetGlideGateDirectDispatchThunkAddress()` host capability from
   the direct-dispatch request in `RunExecutionThread`.
2. When capability is absent, set `direct_glide_dispatch=false` and do not call
   `PatchGlideGatePlanForDirectDispatch`.
3. Verify that base Glide gate image writes, descriptor registration, and
   memory protection proceed through the existing path.
4. Make loader status and `REPIU_LINEXE_INIT_TRACE` distinguish requested and
   actual capability.
5. Run the core probe, the Linux x64 default run, and the `CON` regression path.
6. Identify the next DOS/DPMI frontier and update design, analysis, and work
   log documents.

### Done criteria

* The Linux x64 default run reports `direct=0`, `glide_fits=1`, and `active=1`.
* `AX=FF00h` returns `EAX=0000FFFFh` with `GS=0x20`.
* `CON` handle `0x0005` open/write remains intact.
* Existing patch/verification behavior on capable direct-dispatch hosts is
  preserved.
* The core probe passes `24/24`.
* The real-run result and next frontier are recorded in the work log.

### Prohibitions

* Do not force a 64-bit host pointer into the 32-bit guest image.
* Do not add a guest-EIP exception or original-binary patch.
* Do not fail the whole LINEXE environment merely because direct dispatch is
  unavailable.
