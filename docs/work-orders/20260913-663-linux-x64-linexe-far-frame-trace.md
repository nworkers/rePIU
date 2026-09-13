# Task 663 작업 지시: Linux x64 LINEXE far-transfer 프레임 추적

## 한국어

### 작업 범위

1. Task 662의 `0x010F0232` cache breakpoint 재진입 결과와 Task 661의 4바이트 ESP 차이를 기준으로 LINEXE 경계 추적 범위를 확정합니다.
2. `REPIU_LINEXE_FAR_TRANSFER_TRACE` 파서를 추가하고, `HandleLinexeFarTransferBoundary` 및 `HandleFarJumpInstruction`에 bounded trace를 연결합니다.
3. trace에는 입력/출력 EIP·ESP·EBP·EDI와 제한된 stack word를 포함하되 guest state를 수정하지 않습니다.
4. Linux x64 Debug 빌드와 `repiu_core_probe`를 실행합니다.
5. bounded runtime trace를 실행해 실제 프레임 소비 경계를 확인합니다.
6. analysis와 작업 로그를 갱신하고 작업 단위를 커밋합니다.

### 구현 순서

1. [x] 관련 AOT reentry, LINEXE boundary, far-jump HLE 코드를 재확인합니다.
2. [ ] 설계 문서에 정의한 opt-in bounded trace를 구현합니다.
3. [ ] Linux x64 Debug `repiu` 및 `repiu_core_probe`를 빌드합니다.
4. [ ] `REPIU_LINEXE_FAR_TRANSFER_TRACE=1` bounded runtime을 실행합니다.
5. [ ] 확인 결과와 남은 blind spot을 analysis/work-log에 기록합니다.
6. [ ] 관련 변경을 하나의 Git 커밋으로 남깁니다.

### 완료 조건

- 환경 변수가 없을 때 기본 출력과 guest semantics가 변하지 않습니다.
- trace가 LINEXE boundary의 실제 입력 및 성공 복귀 프레임 소비를 bounded하게 기록합니다.
- far-jump HLE fallback 여부를 trace로 구분할 수 있습니다.
- Debug 빌드와 core probe가 통과합니다.
- 4바이트 ESP 차이의 위치가 확인됨 또는 아직 미확정임을 문서에 명시합니다.

## English

### Scope

1. Fix the LINEXE boundary trace scope from Task 662's cache-breakpoint
   re-entry result and Task 661's four-byte ESP difference.
2. Add the `REPIU_LINEXE_FAR_TRANSFER_TRACE` opt-in and bounded traces to
   `HandleLinexeFarTransferBoundary` and `HandleFarJumpInstruction`.
3. Include input/output EIP, ESP, EBP, EDI, and a bounded stack window without
   modifying guest state.
4. Build Linux x64 Debug `repiu` and `repiu_core_probe`.
5. Run a bounded runtime trace to identify the actual frame-consumption
   boundary.
6. Update analysis and the work log, then commit the task unit.

### Implementation order

1. [x] Reconfirm the AOT re-entry, LINEXE boundary, and far-jump HLE paths.
2. [x] Implement the opt-in bounded trace from the design.
3. [x] Build Linux x64 Debug `repiu` and `repiu_core_probe`.
4. [x] Run a bounded runtime with `REPIU_LINEXE_FAR_TRANSFER_TRACE=1`.
5. [x] Record confirmed findings and remaining blind spots in analysis/work log.
6. [x] Leave one Git commit for the task unit.

### Completion criteria

- No environment variable preserves default output and guest semantics.
- The trace records bounded input and successful frame consumption at the
  LINEXE boundary.
- The trace distinguishes a far-jump HLE fallback.
- Debug build and core probe pass.
- Documentation states whether the four-byte ESP delta was localized.
