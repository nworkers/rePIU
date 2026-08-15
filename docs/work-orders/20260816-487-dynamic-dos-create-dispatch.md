# Dynamic DOS 파일 생성 디스패치 작업 지시

관련 설계: [Dynamic DOS 파일 생성 디스패치 설계](../design/20260816-487-dynamic-dos-create-dispatch.md)

## 작업

1. `HandleTracedDosInterrupt21()`에서 `AH=3Ch`를 공용 DOS INT 21h 처리기로 전달합니다.
2. `dos_file_create_probe`에 추적용 디스패처를 통과하는 생성 요청 검증을 추가합니다.
3. Win32 Release 빌드와 `repiu_aot_probe`로 회귀를 확인합니다.
4. architecture, 누적 analysis, 작업 로그에 원인과 결과를 반영합니다.

## 완료 조건

* `dynamic` 경로의 `AH=3Ch`가 unsupported 분기로 가지 않습니다.
* 공용 파일 생성 구현이 호출되고 DOS 반환 상태와 EIP가 올바릅니다.
* 기존 probe가 모두 통과합니다.

추가로 DOS close가 read/write stream cache를 모두 해제하게 하고, probe에서 임시 root
cleanup 성공도 확인합니다.

# Dynamic DOS File-Create Dispatch Work Order

Related design: [Dynamic DOS File-Create Dispatch Design](../design/20260816-487-dynamic-dos-create-dispatch.md)

## Work

1. Route `AH=3Ch` from `HandleTracedDosInterrupt21()` to the common DOS INT 21h handler.
2. Make DOS close release both read and write stream caches.
3. Add traced-dispatch creation and cleanup checks to `dos_file_create_probe`.
4. Run the Win32 Release build and `repiu_aot_probe` regression suite.
5. Record the cause and result in architecture, cumulative analysis, and the work log.

## Completion criteria

* The `dynamic` `AH=3Ch` path does not reach the unsupported branch.
* The common create implementation runs with correct DOS return state and EIP.
* Existing probes pass.
