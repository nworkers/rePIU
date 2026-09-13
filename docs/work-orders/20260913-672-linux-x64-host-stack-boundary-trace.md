# 작업 지시서: Linux x64 host stack 경계 추적

## 한국어

### 배경

Task 671의 공통 AOT 이미지 감사는 host `RSP`를 변경하는 미변환 guest 명령을 찾지 못했다. 실행 fault는 ReturnThunk 진입 전에 native `RSP`가 저주소가 되었음을 보여준다. 원인을 특정하기 위해 세 assembly 경계의 native `RSP`를 기록한다.

### 구현 순서

1. 설계 문서의 세 지점에 capture 전역값을 추가한다.
2. Linux x64 guest entry와 ReturnThunk assembly에서 값을 저장한다.
3. Linux fault report가 세 값을 함께 출력하도록 연결한다.
4. WSL 빌드와 bounded 실행으로 값을 수집한다.
5. 결과를 작업 로그와 `docs/analysis/` 누적 문서에 반영한다.

### 완료 조건

* 정상 빌드가 완료된다.
* fault report에 entry/cache-call/ReturnThunk `RSP`가 표시된다.
* 수집은 진단 전용이며 guest 실행 의미를 바꾸지 않는다.

## English

### Background

Task 671's shared AOT-image audit found no untranslated guest instruction that changes host `RSP`. The runtime fault shows native `RSP` already low before ReturnThunk's first stack use. Capture native `RSP` at three assembly boundaries to locate the transition.

### Implementation order

1. Add capture globals named by the three design points.
2. Store them from the Linux x64 guest-entry and ReturnThunk assembly.
3. Include all three values in the Linux fault report.
4. Build through WSL and collect bounded runtime evidence.
5. Record the result in the work log and cumulative `docs/analysis/` material.

### Done when

* The affected Linux build succeeds.
* The fault report includes guest-entry, cache-call, and ReturnThunk `RSP` values.
* The change remains diagnostic-only and does not alter guest semantics.
