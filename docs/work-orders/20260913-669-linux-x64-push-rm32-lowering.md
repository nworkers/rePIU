# Task 669 작업 지시: Linux x64 PUSH r/m32 guest-stack lowering

## 한국어

### 작업 항목

1. `FF /6` bare `PUSH r/m32`를 공통 `kStackSequence` lowering 대상으로
   분류합니다.
2. register source와 memory source를 guest `R15D` stack 기준으로 내립니다.
3. memory source는 guest ESP decrement 전에 읽고, `[ESP+disp]`도 원본
   주소 계산 순서를 유지하도록 처리합니다.
4. absolute disp32 source는 SIB absolute form으로 재작성합니다.
5. segment/operand-size prefix는 추측하지 않고 기존 boundary 정책을 유지합니다.
6. compatibility probe에 register, 일반 memory, ESP-based memory, absolute
   memory 사례와
   lowered byte decode 검증을 추가합니다.
7. Linux x64 `repiu`, core probe, bounded `pumpit2a`를 실행합니다.
8. 설계·누적 분석·작업 로그를 갱신합니다.

### 금지 사항

- 특정 guest EIP/ESP 예외처리를 추가하지 않습니다.
- DOS path 또는 HLE 결과를 우회하는 보정값을 추가하지 않습니다.
- `PUSH r/m32`를 host RSP에 위임하지 않습니다.
- `PUSH m16`, segment override, `CALL/JMP` FF group을 이 작업에서 섞어
  처리하지 않습니다.

### 완료 조건

- `FF /6`가 일반 memory prefix 경로가 아닌 guest-stack sequence로 분류됩니다.
- x64 lowered bytes가 source 값을 guest stack에 32-bit로 저장하고 flags를
  보존합니다.
- F0B50의 AH=43h 입력 path가 정상적으로 구성되는지 runtime으로 확인됩니다.
- 빌드·probe·runtime 결과와 남은 문제가 문서에 남습니다.

## English

### Work items

1. Classify bare `FF /6 PUSH r/m32` as a common `kStackSequence` lowering.
2. Lower register and memory sources against the guest `R15D` stack.
3. Load memory sources before decrementing guest ESP and preserve the original
   address order for `[ESP+disp]` sources.
4. Rewrite absolute disp32 memory sources to the SIB absolute form.
5. Keep prefixed forms at the existing fail-closed boundary rather than guessing.
6. Extend the compatibility probe with register, ordinary-memory, ESP-based,
   and absolute-memory cases and lowered-byte decode checks.
7. Run the Linux x64 `repiu` build, core probe, and bounded `pumpit2a` runtime.
8. Update design, cumulative analysis, and the work log.

### Prohibitions

- Do not add a guest EIP/ESP-specific exception.
- Do not bypass DOS path or HLE results with a correction value.
- Do not delegate `PUSH r/m32` to host RSP.
- Do not combine PUSH m16, segment overrides, or the CALL/JMP FF group with
  this task.

### Completion criteria

- `FF /6` is classified as a guest-stack sequence rather than ordinary memory
  prefix lowering.
- The x64 sequence stores the source value as a 32-bit guest-stack dword and
  preserves flags.
- Runtime confirms that F0B50's AH=43h input path is constructed correctly.
- Build, probe, runtime results, and remaining issues are documented.
