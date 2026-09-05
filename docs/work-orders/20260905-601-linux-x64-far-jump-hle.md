# 작업 지시 20260905-601 — Linux x64 guest `66 EA` far jump HLE

## 목표

관측된 `66 EA 04 00 2C 00` guest far jump를 selector table 기반 linear EIP
이전으로 처리합니다.

## 작업

1. `66 EA off16 sel16` decode/HLE를 instruction-emulation 계층에 추가합니다.
2. selector table 변환이 성공할 때만 EIP를 target으로 갱신합니다.
3. valid/missing/out-of-limit selector 변환을 core probe로 검증합니다.
4. Linux x64 build, core probe, probe-success runtime을 실행합니다.
5. 분석과 작업 로그에 새 frontier 및 ABI 미확정 상태를 기록합니다.

## 완료 기준

* `002C:0004`가 `0x01100004`로 변환됩니다.
* ESP와 EFLAGS가 보존됩니다.
* `0x010F016B` SIGILL이 사라집니다.
* Linux x64 build와 core probe가 통과합니다.

---

# Work order 20260905-601 — Linux x64 guest `66 EA` far-jump HLE

## Goal

Handle observed guest far jump `66 EA 04 00 2C 00` as selector-table-based
linear EIP transfer.

## Work

1. Add `66 EA off16 sel16` decode/HLE in instruction emulation.
2. Update EIP only after successful selector-table translation.
3. Cover valid, missing, and out-of-limit selector conversions in a core probe.
4. Run Linux x64 build, core probe, and probe-success runtime.
5. Record the new frontier and continued ABI uncertainty in analysis/work log.

## Done criteria

* `002C:0004` translates to `0x01100004`.
* ESP and EFLAGS are preserved.
* SIGILL at `0x010F016B` disappears.
* Linux x64 build and core probe pass.
