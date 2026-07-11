# DOS 파일 속성 작업 지시

1. VFS path resolve를 재사용하는 query/set API와 session overlay를 추가합니다.
2. `AH=43h AL=00h/01h` register/CF/error contract를 구현합니다.
3. Win32 x86 빌드와 PIU 실행으로 다음 frontier를 확인합니다.
4. 분석 및 작업 로그를 갱신하고 커밋합니다.

# DOS File-Attribute Work Order

Add VFS query/set APIs with a session overlay, implement `AH=43h AL=00h/01h`, build Win32 x86, observe the next frontier, document, and commit.
