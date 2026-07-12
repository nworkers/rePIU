# DOS file I/O ring trace 작업 지시 / Work Order

## 한국어

1. 실행 결과에 64-entry DOS file I/O observation을 추가한다.
2. read와 seek 성공·실패를 공통 sequence로 기록한다.
3. read 결과의 선두 16바이트를 bounded copy한다.
4. loader가 시간순으로 ring을 출력하도록 한다.
5. PIU를 실행하여 `PIU.DAT`의 PTX 판정 직전 I/O를 확인한다.

## English

1. Add a 64-entry DOS file-I/O observation to execution results.
2. Record successful and failed reads and seeks in one sequence.
3. Copy at most the first 16 result bytes for each read.
4. Print the ring chronologically from the loader.
5. Run PIU and inspect I/O immediately preceding the `PIU.DAT` PTX check.
