# 실행 probe 메모리 구간 추출 절차

guest 실행 중 특정 코드 주소에 처음 도달한 시점의 메모리 구간을 파일로 받아 host 도구로
검증하는 절차입니다. 설계는 `docs/design/20260812-474-execution-probe-memory-dump.md`,
적용 사례는 `docs/analysis/pumpit8-bga-iccp-crash.md`에 있습니다.

## 환경 변수

| 변수 | 의미 |
|---|---|
| `REPIU_EXECUTION_PROBE_OFFSET` | 관측할 guest runtime offset. 이 값이 없으면 dump도 동작하지 않습니다. |
| `REPIU_EXECUTION_PROBE_DUMP_PATH` | 출력 파일 경로. 필수. |
| `REPIU_EXECUTION_PROBE_DUMP_BYTES` | 복사할 바이트 수. 필수. 최대 1 MiB. |
| `REPIU_EXECUTION_PROBE_DUMP_BASE` | `eax` `ebx` `ecx` `edx` `esi` `edi` `ebp` `esp` 중 하나이거나 절대 주소. 기본값 `eax`. |
| `REPIU_EXECUTION_PROBE_DUMP_OFFSET` | base에 더할 양수 offset. 기본값 0. |
| `REPIU_EXECUTION_PROBE_DUMP_INDIRECT` | `1`이면 `base+offset`의 4바이트를 읽어 그 값을 시작 주소로 사용합니다. |

## 주의

* `REPIU_EXECUTION_TIMEOUT_MS`를 설정하면 1초 무진척 정지 감지기가 함께 켜집니다. 게임이
  잠시 조용해지는 지점을 지나야 한다면 값을 주지 말고 기본값(무제한)으로 실행합니다.
* 전체 구간이 guest arena 안에서 읽을 수 있어야 복사합니다. 일부만 읽히는 범위는 복사하지
  않고 실패로 남습니다.
* 파일 기록은 guest thread가 멈춘 뒤에 수행되므로, 실행이 정상 종료되거나 예외로 잡혀야
  파일이 생깁니다.

## 절차

1. 관측 지점의 레지스터를 먼저 확인합니다. offset만 지정해 실행하면 loader 진단이 7개
   레지스터가 가리키는 32바이트 창을 출력합니다.

   ```powershell
   $env:REPIU_EXECUTION_PROBE_OFFSET = "0xE49F8"
   $env:REPIU_EXECUTION_TIMEOUT_MS = "0"
   .\build\win32_x86_debug\Debug\repiu.exe pumpit8 2>&1 | Tee-Object run.log
   ```

2. 원하는 buffer를 가리키는 레지스터를 base로 지정해 구간을 추출합니다.

   ```powershell
   $env:REPIU_EXECUTION_PROBE_DUMP_BASE = "ebx"
   $env:REPIU_EXECUTION_PROBE_DUMP_BYTES = "0xA37"
   $env:REPIU_EXECUTION_PROBE_DUMP_PATH = "$PWD\chunk.bin"
   ```

3. 포인터가 구조체 안에 있으면 indirect를 사용합니다. 아래는 `eax+0xC`에 저장된 포인터가
   가리키는 곳에서 1 MiB를 받는 예입니다.

   ```powershell
   $env:REPIU_EXECUTION_PROBE_DUMP_BASE = "eax"
   $env:REPIU_EXECUTION_PROBE_DUMP_OFFSET = "0xC"
   $env:REPIU_EXECUTION_PROBE_DUMP_INDIRECT = "1"
   $env:REPIU_EXECUTION_PROBE_DUMP_BYTES = "0x100000"
   ```

4. loader 진단에서 결과를 확인합니다.

   ```text
   Win32 execution probe dump captured/written/base/source/bytes: true/true/0x0525EC6C/0x04A88418/0x00100000
   ```

# Execution Probe Memory Range Extraction Procedure

This procedure captures a guest memory range to a file at the first arrival at a chosen code
address, for validation with host tools. The design is in
`docs/design/20260812-474-execution-probe-memory-dump.md`, and a worked application is in
`docs/analysis/pumpit8-bga-iccp-crash.md`.

## Environment Variables

| Variable | Meaning |
|---|---|
| `REPIU_EXECUTION_PROBE_OFFSET` | Guest runtime offset to observe. Without it the dump does nothing. |
| `REPIU_EXECUTION_PROBE_DUMP_PATH` | Output file path. Required. |
| `REPIU_EXECUTION_PROBE_DUMP_BYTES` | Bytes to copy. Required. Maximum 1 MiB. |
| `REPIU_EXECUTION_PROBE_DUMP_BASE` | One of `eax` `ebx` `ecx` `edx` `esi` `edi` `ebp` `esp`, or an absolute address. Defaults to `eax`. |
| `REPIU_EXECUTION_PROBE_DUMP_OFFSET` | Positive offset added to the base. Defaults to zero. |
| `REPIU_EXECUTION_PROBE_DUMP_INDIRECT` | When `1`, read four bytes at `base+offset` and start there. |

## Cautions

* Setting `REPIU_EXECUTION_TIMEOUT_MS` also arms a one-second no-progress stall detector. To run
  past a point where the guest goes briefly quiet, leave it unset and use the unlimited default.
* The copy happens only when the complete range is readable inside the guest arena. A partially
  readable range is left as a failure rather than copied.
* The file is written after the guest thread stops, so execution must end normally or through a
  caught exception for the file to appear.

## Procedure

1. Identify the registers at the observation point first. Running with only the offset set makes
   the loader diagnostics print 32-byte windows for seven registers.

   ```powershell
   $env:REPIU_EXECUTION_PROBE_OFFSET = "0xE49F8"
   $env:REPIU_EXECUTION_TIMEOUT_MS = "0"
   .\build\win32_x86_debug\Debug\repiu.exe pumpit8 2>&1 | Tee-Object run.log
   ```

2. Extract the range using the register that addresses the buffer of interest.

   ```powershell
   $env:REPIU_EXECUTION_PROBE_DUMP_BASE = "ebx"
   $env:REPIU_EXECUTION_PROBE_DUMP_BYTES = "0xA37"
   $env:REPIU_EXECUTION_PROBE_DUMP_PATH = "$PWD\chunk.bin"
   ```

3. Use indirect mode when the pointer lives inside a structure. This example takes 1 MiB from
   wherever the pointer stored at `eax+0xC` points.

   ```powershell
   $env:REPIU_EXECUTION_PROBE_DUMP_BASE = "eax"
   $env:REPIU_EXECUTION_PROBE_DUMP_OFFSET = "0xC"
   $env:REPIU_EXECUTION_PROBE_DUMP_INDIRECT = "1"
   $env:REPIU_EXECUTION_PROBE_DUMP_BYTES = "0x100000"
   ```

4. Confirm the outcome in the loader diagnostics.

   ```text
   Win32 execution probe dump captured/written/base/source/bytes: true/true/0x0525EC6C/0x04A88418/0x00100000
   ```
