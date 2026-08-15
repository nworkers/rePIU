# Dynamic DOS 파일 생성 디스패치 설계

## 관측과 원인

`pumpit8`의 `dynamic` 실행이 `ERRLOG.txt` 열기 실패 뒤 `INT 21h AH=3Ch`를 호출하고
`unsupported DOS INT 21h AH=0x3c`로 종료했습니다. Task 477에서 파일 생성 계층과 공용
`HandleDosInterrupt21()`의 `AH=3Ch` 처리는 구현했지만, `enable_dos_hle=false`인
`dynamic` backend가 먼저 사용하는 `HandleTracedDosInterrupt21()`의 허용 목록에는
`0x3C`가 없습니다. 따라서 구현된 서비스에 도달하지 못합니다.

## 설계

추적용 디스패처에 파일 생성 의미를 복제하지 않습니다. `AH=3Ch`를 기존 `AH=09h`와
같이 공용 `HandleDosInterrupt21()`로 위임합니다. 공용 처리기가 interrupt 계수, 경로
검증, VFS 생성, DOS 반환값, EIP 진행을 모두 한 번만 담당합니다.

```mermaid
flowchart LR
    G["guest INT 21h AH=3Ch"] --> T["HandleTracedDosInterrupt21"]
    T --> C["HandleDosInterrupt21"]
    C --> V["CreateDosFile / virtual filesystem"]
```

별도 backend별 파일 생성 구현이나 target별 예외는 추가하지 않습니다. 이는 원본 DOS
호출을 공용 HLE 서비스로 전달하는 통합 수정이며 게임 로직을 바꾸지 않습니다.

## 검증

기존 `dos_file_create_probe`에 실제 `CD 21` 명령과 guest 경로를 둔 작은 runtime 범위를
구성하고 `HandleTracedDosInterrupt21()`를 직접 호출하는 검증을 추가합니다. 성공 시
EIP가 2바이트 진행하고, CF가 지워지며, AX에 handle 5가 반환되고, 파일 생성 계수가
증가하며 host 파일이 존재해야 합니다. 그 뒤 Win32 Release 빌드와 probe를 실행합니다.

검증 과정에서 확인한 기존 close 경로의 write stream cache도 read cache와 함께 해제하고,
임시 root 삭제 성공을 회귀 조건으로 둡니다.

# Dynamic DOS File-Create Dispatch Design

## Observation and cause

The `pumpit8` `dynamic` run attempted `INT 21h AH=3Ch` after opening
`ERRLOG.txt` failed, then stopped with `unsupported DOS INT 21h AH=0x3c`.
Task 477 implemented file creation and the common `HandleDosInterrupt21()`
case, but the allow-list in `HandleTracedDosInterrupt21()`—used first by the
`dynamic` backend with `enable_dos_hle=false`—does not contain `0x3C`.

## Design

Do not duplicate file-create semantics in the traced dispatcher. Route `AH=3Ch`
to the common `HandleDosInterrupt21()` just like the existing `AH=09h` route.
The common handler remains the single owner of accounting, path validation, VFS
creation, DOS return values, and EIP advancement. No target-specific exception
or second backend implementation is introduced.

## Verification

Extend `dos_file_create_probe` with a small guest runtime range containing a
real `CD 21` instruction and path, then call `HandleTracedDosInterrupt21()`.
Require a two-byte EIP advance, clear CF, handle 5 in AX, one create operation,
and the resulting host file. Release the existing write-stream cache alongside
the read cache on close, and require successful scratch-root cleanup as a
regression condition. Then build Win32 Release and run the probe.
