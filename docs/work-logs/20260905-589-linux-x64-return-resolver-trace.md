# 작업 로그 20260905-589 — Linux x64 return resolver trace

설계: [20260905-589](../design/20260905-589-linux-x64-return-resolver-trace.md)  
작업 지시: [20260905-589](../work-orders/20260905-589-linux-x64-return-resolver-trace.md)

## 결과

`REPIU_LINUX_X64_RETURN_TRACE`가 있을 때만 `LinuxX64EngineResolver`가
`invalid-state`, `cache-miss`, `cache-hit`과 guest source/cache target을 기록하도록
추가했습니다. 기본 실행은 출력과 제어 흐름이 변하지 않습니다.

`pumpit2a`는 `source=0x010F4AD1`, `result=cache-miss`를 기록했습니다. return thunk와
resolver는 설치되어 있으며, `FindAotCacheAddress`가 이 guest return address의 target을
찾지 못해 zero를 답한 것이 Task 588 INT3의 직접 원인입니다.

## 검증

* WSL Linux x64 `repiu` 빌드 성공. 기존 `g_repiu_active_thread_context`의 initialized
  `extern` GCC warning 하나만 있었습니다.
* `REPIU_GUEST_WATCH=0x010F4A96 REPIU_LINUX_X64_RETURN_TRACE=1 timeout 5s`
  실행에서 `cache-miss source=0x010F4AD1`와 기존 fail-closed INT3/SIGILL 순서를 확인했습니다.

## 후속 작업

`0x010F4AD1`의 원본 instruction과 AOT address-map coverage를 분석하고, Linux x64
return cache miss를 raw guest execution 없이 처리할 재진입 정책을 설계합니다.

---

# Work log 20260905-589 — Linux x64 return-resolver trace

Design: [20260905-589](../design/20260905-589-linux-x64-return-resolver-trace.md)  
Work order: [20260905-589](../work-orders/20260905-589-linux-x64-return-resolver-trace.md)

## Result

When `REPIU_LINUX_X64_RETURN_TRACE` is set, `LinuxX64EngineResolver` now logs
`invalid-state`, `cache-miss`, or `cache-hit` with the guest source and cache
target. The default run changes neither output nor control flow.

`pumpit2a` recorded `source=0x010F4AD1`, `result=cache-miss`. The return thunk
and resolver are installed; `FindAotCacheAddress` cannot resolve that guest
return address, so zero is the direct cause of Task 588's fail-closed INT3.

The original executable bytes around `0x010F4AC0` are
`... 1E 07 52 FF D0 5A C6 03 02 ...`. Hence `0x010F4AD1` is `pop edx`, directly
after indirect `call eax`; the miss is a return continuation inside a guest
basic block, not an arbitrary target.

## Verification

* WSL Linux x64 `repiu` built successfully, with only the existing initialized
  `extern g_repiu_active_thread_context` GCC warning.
* The five-second watched run with the trace environment variable recorded the
  cache miss and the established INT3/SIGILL sequence.

## Follow-up

Analyze the original instruction and AOT address-map coverage for `0x010F4AD1`,
then design a return-cache-miss reentry policy without raw guest execution.
