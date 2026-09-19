# Task 721 작업 지시 — Linux x64 정지 구간 guest 위치 census

설계: [20260919-721](../design/20260919-721-linux-x64-stall-position-census.md)

## 범위

Linux x64의 opt-in wall-clock guest-position census를 활성화하여 로딩 정지 중 guest EIP
분포를 수집합니다. 기본 실행과 Win32 x86 동작은 바꾸지 않습니다.

## 단계

1. native-phase capture architecture guard에 Linux x64 opt-in을 추가합니다.
2. Linux signal callback은 read-only register capture만 수행하고 host stack scan을 하지
   않게 합니다.
3. Linux/Win32 core probe와 Linux 기본·관찰 실행을 검증합니다.
4. 정지 위치 분포와 다음 frontier를 analysis 및 작업 로그에 기록합니다.

## 제외 범위

- guest clock, safe-point injection, Glide dispatch의 동작 변경
- signal handler에서 native context write-back
- Linux host-stack symbolization 또는 stack walk

## English

Design: [20260919-721](../design/20260919-721-linux-x64-stall-position-census.md)

### Scope

Enable an opt-in wall-clock guest-position census on Linux x64 to collect guest-EIP
distributions during loading stalls. Default execution and Win32 x86 behavior remain
unchanged.

### Steps

Add the Linux x64 opt-in to the native-phase capture guard; limit its signal callback
to read-only register capture with no host stack scan; verify Linux/Win32 core probes
and Linux default/observation runs; record the resulting stall distribution and next
frontier.

### Out of scope

Changing the guest clock, safe-point injection, or Glide dispatch; native-context
write-back from the signal handler; Linux host-stack symbolization or stack walking.
