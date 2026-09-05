# 작업 지시 20260905-600 — Linux x64 guest `PUSH CS` HLE

## 목표

Linux x64 long mode에서 invalid인 guest `PUSH CS`를 32-bit guest stack 의미로
처리하여 Task 599 성공 경로 탐침을 다음 frontier까지 진행합니다.

## 작업

1. 현재 EIP가 속한 selector descriptor를 유일하게 찾는 helper를 추가합니다.
2. `HandleSegmentPushInstruction`의 `0Eh` 분기에 helper selector를 사용합니다.
3. descriptor 미확정/중첩에서는 기존 fail-closed를 유지합니다.
4. 범위 선택과 stack write를 검증하는 core probe를 추가하거나 갱신합니다.
5. Linux x64 build, core probe, probe-success runtime을 실행하고 결과를 기록합니다.

## 완료 기준

* `0Eh`가 `ESP`를 4 감소시키고 zero-extended current CS를 기록합니다.
* 다른 segment push와 기본 오류 경로가 변하지 않습니다.
* 기존 `0x010F0117` SIGILL 대신 다음 frontier가 관측됩니다.
* Linux x64 build와 core probe가 통과합니다.

---

# Work order 20260905-600 — Linux x64 guest `PUSH CS` HLE

## Goal

Handle guest `PUSH CS`, invalid in Linux x64 long mode, with 32-bit guest-stack
semantics so the Task 599 success-path probe reaches its next frontier.

## Work

1. Add a helper that uniquely finds the selector descriptor containing current
   EIP.
2. Use that selector for the `0Eh` branch of `HandleSegmentPushInstruction`.
3. Retain fail-closed behavior for absent or overlapping descriptors.
4. Add or update a core probe for selection and stack write.
5. Run Linux x64 build, core probe, and the probe-success runtime; record the
   results.

## Done criteria

* `0Eh` decrements ESP by four and writes zero-extended current CS.
* Other segment pushes and the default error path are unchanged.
* A next frontier replaces the old `0x010F0117` SIGILL.
* Linux x64 build and core probe pass.
