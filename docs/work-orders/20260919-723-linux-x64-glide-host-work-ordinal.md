# Task 723 작업 지시 — Linux x64 Glide host-work ordinal 분해

설계: [20260919-723](../design/20260919-723-linux-x64-glide-host-work-ordinal.md)

## 범위

기존 Linux x64 Glide timing profile을 실행해 `grBufferSwap`, `grLfbLock` 등 host-command
작업의 ordinal별 비중을 확인합니다. 제품 코드와 Win32 구성을 변경하지 않습니다.

## 단계

1. 기존 profile 설정 이름과 summary 형식을 소스에서 확인합니다.
2. 30초 bounded `pumpit2a` 실행으로 완결 summary를 수집합니다.
3. 큰 ordinal을 계산하고, analysis와 작업 로그에 확인됨·미확정을 구분해 기록합니다.

## English

Design: [20260919-723](../design/20260919-723-linux-x64-glide-host-work-ordinal.md)

### Scope

Run existing Linux x64 Glide timing profiles to identify the ordinal share of host-command
work such as `grBufferSwap` and `grLfbLock`. Do not change product code or Win32.

### Steps

Confirm existing setting names and summary format; collect a completed 30-second bounded
`pumpit2a` summary; calculate the largest ordinals and document confirmed versus unresolved
findings in analysis and the work log.
