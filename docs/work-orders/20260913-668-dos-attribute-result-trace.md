# Task 668 작업 지시: DOS AH=43h 파일 속성 결과 추적

## 한국어

### 작업 항목

1. `HandleDosFileAttributes`에 `REPIU_DOS_ATTR_TRACE=1` opt-in 진단을
   추가합니다.
2. query/set subfunction에 대해 guest EIP, path 해석 결과, DOS 오류 코드,
   반환 EAX/ECX/EDX와 CF를 bounded하게 기록합니다.
3. guest path를 읽지 못하는 경우도 성공/실패 원인으로 구분해 기록합니다.
4. 반복 조회가 로그를 폭증시키지 않도록 성공·실패 출력 상한을 분리합니다.
5. Linux x64 빌드, core probe, bounded `pumpit2a` 실행으로 검증합니다.
6. 확인된 AH=43h 결과와 남은 원인을 누적 분석 및 작업 로그에 반영합니다.

### 금지 사항

- 특정 guest EIP 또는 ESP를 조건으로 한 의미론 변경을 추가하지 않습니다.
- 파일명, 확장자, 대소문자, VFS root 정책을 추측으로 수정하지 않습니다.
- EAX=0 또는 반환 주소를 진단 단계에서 다른 값으로 치환하지 않습니다.
- 무제한 DOS trace를 추가하지 않습니다.

### 완료 조건

- trace가 꺼져 있으면 기존 DOS/HLE 동작과 빌드 결과가 유지됩니다.
- trace가 켜져 있으면 AH=43h의 guest/virtual/host 경로와 성공·오류·CF/EAX가
  제한된 출력으로 확인됩니다.
- 실행 결과가 다음 공통 HLE 수정 여부를 판단할 수 있을 정도로 구체적입니다.

## English

### Work items

1. Add the opt-in `REPIU_DOS_ATTR_TRACE=1` diagnostic to
   `HandleDosFileAttributes`.
2. Boundedly record guest EIP, path-resolution results, DOS error code, returned
   EAX/ECX/EDX, and CF for query/set subfunctions.
3. Report guest-path read failures separately from ordinary service failures.
4. Use separate success and failure caps so repeated queries cannot flood the log.
5. Verify with the Linux x64 build, core probe, and bounded `pumpit2a` run.
6. Update cumulative analysis and the work log with the AH=43h result and the
   remaining cause.

### Prohibitions

- Do not change semantics based on a guest EIP or ESP.
- Do not guess and change filename, extension, case, or VFS-root policy.
- Do not replace EAX=0 or any return address during this diagnostic task.
- Do not add an unbounded DOS trace.

### Completion criteria

- Existing DOS/HLE behavior and build output remain intact when tracing is off.
- Tracing shows the AH=43h guest/virtual/host paths and success/error/CF/EAX in
  bounded output.
- The result is specific enough to choose whether a common HLE fix is justified.
