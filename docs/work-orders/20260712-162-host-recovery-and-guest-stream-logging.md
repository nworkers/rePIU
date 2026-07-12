# Host 복귀와 guest stream logging 작업 지시 / Work Order

## 한국어

1. recovery stub의 상태 읽기를 `CS:` override로 변경한다.
2. guest stdout/stderr 누적 버퍼를 분리한다.
3. DOS `AH=09h`, `AH=40h`를 stream별로 라우팅한다.
4. 실행 파일명 logger로 stdout/info와 stderr/error를 줄 단위 출력한다.
5. Win32 x86 빌드 후 PIU 종료와 프로세스 정리를 실제 실행으로 확인한다.

## English

1. Change recovery-stub state reads to use a `CS:` override.
2. Separate guest stdout and stderr accumulation buffers.
3. Route DOS `AH=09h` and `AH=40h` by stream.
4. Log lines through an executable-name logger at stdout/info and stderr/error.
5. Build Win32 x86 and verify PIU termination and process cleanup by execution.
