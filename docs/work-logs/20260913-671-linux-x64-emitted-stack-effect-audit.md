# Task 671 작업 로그: Linux x64 emitted stack-effect audit

## 한국어

instruction census에 opt-in emitted stack-effect audit를 추가했습니다. 정적
plan의 stack-effect 후보를 x64 emitted bytes로 다시 decode하고, 원본 bytes와
함께 출력하도록 했습니다.

결과:

- `long_mode_stack_audit_candidates=1086`
- `decode_failures=0`
- bounded 출력 64개
- mnemonic tally: `pop=181`, `popfq=362`, `push=362`, `pushfq=181`
- 일반 host `CALL`/`RET`/`MOV ESP` emitted 후보는 발견되지 않음

기존 census target의 OpenGL link 누락 때문에 실행 파일은 동일 object와
라이브러리를 사용한 직접 link workaround로 검증했습니다. 이 link 문제는 이번
작업에서 수정하지 않았습니다.

## English

Added an opt-in emitted stack-effect audit to the instruction census. It
re-decodes x64 emitted bytes for static-plan stack-effect candidates and prints
the original and emitted windows together.

Results:

- `long_mode_stack_audit_candidates=1086`
- `decode_failures=0`
- 64 bounded candidate lines printed
- Mnemonics: `pop=181`, `popfq=362`, `push=362`, `pushfq=181`
- No ordinary emitted host `CALL`/`RET`/`MOV ESP` candidate was found

The existing census target still has a missing-OpenGL-symbol link issue, so
verification used a direct link of the same object and libraries. That issue
was not changed here.
