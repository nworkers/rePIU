# MSCDEX INT 2Fh 설치 확인 작업 지시

1. `HandleDosInterrupt2F`에 관찰된 `AX=1500h` 분기를 추가합니다.
2. MSCDEX 미설치 계약으로 `BX=0`, `CX=0`을 반환합니다.
3. interrupt 분석과 KB에 호출 의미·외부 근거를 갱신합니다.
4. Win32 x86 Debug 빌드와 실제 asset 실행으로 다음 frontier를 확인합니다.
5. Glide 실행을 의사결정 지점까지 계속 확장합니다.
6. 작업 로그를 작성하고 커밋합니다.

# MSCDEX INT 2Fh Installation Probe Work Order

Add the observed `AX=1500h` branch to `HandleDosInterrupt2F`; return the no-MSCDEX contract `BX=0`, `CX=0`; update interrupt analysis and KB with authoritative context; build and run Win32 x86 to the next frontier; continue Glide implementation until a decision is required; write the work log; and commit.
