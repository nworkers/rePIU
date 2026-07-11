# LINEXE stack-local far pointer 보존 작업 지시

1. `object2+E38B9`와 `object2+E395F`의 stack-local 값을 관찰합니다.
2. 최초 불일치가 발생하는 공용 명령 처리 경로를 수정합니다.
3. export loop 진입과 export resolve 진행을 확인합니다.
4. Win32 x86 빌드와 supervisor 실행으로 검증하고 분석·작업 로그를 갱신합니다.

# LINEXE Stack-Local Far-Pointer Preservation Work Order

Observe stack locals at `object2+E38B9` and `object2+E395F`, repair the first incorrect shared instruction boundary, verify export-loop progress, then build and document the result.
