# LINEXE procedure-resolution frontier design

## 목적 / Purpose

HLE로 처리되지 않은 Glide procedure lookup이 원본 LINEXE resolver의 null 문자열 비교로
흐르는지 확인하고, 요청 이름을 잃지 않고 기록합니다.

Determine whether an unhandled Glide procedure lookup falls through to the original
LINEXE resolver's null-string comparison, while preserving the requested name.

## 정책 / Policy

virtual Glide module handle에 대한 모든 `GETPROCADDR` 요청은, HLE gate가 존재하는지와
무관하게 마지막 요청 이름을 telemetry에 먼저 기록합니다. 알려진 gate만 기존처럼 성공
반환하며, 알 수 없는 이름은 원래 fallback을 유지해 관측만 합니다.

For every `GETPROCADDR` request against the virtual Glide module handle, record the last
requested name before checking whether an HLE gate exists. Known gates return normally as
before; unknown names retain the existing fallback solely for observation.
