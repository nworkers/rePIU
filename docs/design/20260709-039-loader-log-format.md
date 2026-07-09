# loader 로그 포맷 설계

## 배경

Win32 loader 로그는 현재 `[level] [name] log` 형태로 출력된다.
warn/error 분류가 추가되면서 현재 blocker는 구분되지만, timestamp가 없고 level 문자열 폭이 달라 로그를 빠르게 스캔하기 어렵다.

## 목표

* loader 로그를 `[%X.%e] [level] [name] log` 형태로 출력한다.
* millisecond 단위 timestamp를 포함한다.
* level 필드는 모든 level 이름이 같은 폭으로 보이도록 오른쪽 정렬한다.
* 로그 레벨 분류 정책은 유지한다.

## 설계

`spdlog` pattern을 다음 형태로 설정한다.

```text
[%X.%e] [%8l] [%n] %v
```

`%X`는 현재 locale 기준 시각의 `HH:MM:SS`이고, `%e`는 millisecond이다.
`%8l`은 level 이름을 8칸 오른쪽 정렬로 출력한다.
8칸은 `critical`까지 포함할 수 있는 폭이며, `info`, `warning`, `error`가 같은 열에 맞춰진다.

## 검증

* `scripts/test_all.ps1`가 계속 성공해야 한다.
* 출력 예시는 다음 형태여야 한다.

```text
[14:30:10.123] [    info] [loader] ...
[14:30:10.124] [ warning] [loader] ...
[14:30:10.125] [   error] [loader] ...
```

# Loader Log Format Design

## Background

Win32 loader logs currently use the `[level] [name] log` shape.
The warn/error classification makes the current blocker easier to identify, but there is no timestamp and level names have different widths, making logs harder to scan quickly.

## Goals

* Print loader logs as `[%X.%e] [level] [name] log`.
* Include millisecond timestamps.
* Right-align the level field to the same width for all level names.
* Preserve the existing log level classification policy.

## Design

Set the `spdlog` pattern to:

```text
[%X.%e] [%8l] [%n] %v
```

`%X` is the current locale time as `HH:MM:SS`, and `%e` is milliseconds.
`%8l` prints the level name right-aligned in an 8-character field.
Eight characters are enough to include `critical`, so `info`, `warning`, and `error` align in the same column.

## Verification

* `scripts/test_all.ps1` must continue to pass.
* Output should look like:

```text
[14:30:10.123] [    info] [loader] ...
[14:30:10.124] [ warning] [loader] ...
[14:30:10.125] [   error] [loader] ...
```
