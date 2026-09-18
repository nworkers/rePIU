# Task 716 작업 지시 — long mode에서 `CS:` 데이터 접근을 복사로 방출

설계: [20260918-716](../design/20260918-716-long-mode-cs-data-boundary.md)

## 범위

Linux x64에서 safe point 주입을 켰을 때 게스트 `0x010F74A1`
(`mov al, cs:[edx+table]`)의 HLE boundary `int3`로 죽는 문제를 고친다. Win32 동작은
바꾸지 않는다.

## 단계

1. 분류기: CS만 쓰는 메모리 operand를 `kAddressSizePrefix`로 판정
2. emitter: long-mode 분기에서 CS 데이터 boundary를 복사 + fallthrough 점프로 방출
3. HLE 커버리지 검증이 그 복사를 인식
4. 분류기·emitter probe
5. 두 host 빌드와 core probe, Linux off/on 실행, Win32 30초 실행
6. 작업 로그, frontier

## 검증

* Linux x64 core probe 30/30, Win32 core probe 28/28
* Linux off 실행 회귀 없음, on 실행에서 `0x010F74A1` 크래시 소멸
* Win32 30초 실행이 이전과 같음

---

## English

Design: [20260918-716](../design/20260918-716-long-mode-cs-data-boundary.md)

### Scope

Fix the Linux x64 crash, with safe-point injection on, on the HLE boundary `int3`
for guest `0x010F74A1` (`mov al, cs:[edx+table]`). Win32 behavior does not change.

### Steps

1. Classifier: judge a CS-only memory operand `kAddressSizePrefix`.
2. Emitter: in the long-mode branch, emit a CS data boundary as a copy plus a
   fallthrough jump.
3. HLE coverage validation recognizes that copy.
4. Classifier and emitter probes.
5. Builds and core probes on both hosts, Linux runs with injection off and on, and
   a 30-second Win32 run.
6. Work log and frontier.

### Verification

* Linux x64 core probe 30 of 30, Win32 28 of 28.
* No regression with injection off on Linux; with it on, the `0x010F74A1` crash is
  gone.
* A 30-second Win32 run is unchanged.
