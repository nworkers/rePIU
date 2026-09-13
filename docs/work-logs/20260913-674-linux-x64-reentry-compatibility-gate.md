# Task 674 작업 로그: Linux x64 reentry compatibility gate

## 한국어

x64 reentry 시 `CanResumeLinuxX64LegacyTarget`를 기준으로 원본 bytes를
실행해도 의미가 같은지 공통 판정하도록 했습니다.

- non-identical 일반 continuation은 TF와 reentry state를 해제하고 fail-closed
- planner HLE는 guest 주소에서 공통 HLE dispatcher로 직접 전달
- `FF /2`, `FF /4`, `RET`, branch-shaped boundary는 기존 transfer handler로 전달
- handler가 없는 비동일 경계는 다른 cache target을 resolve하지 못하면 거부

이로써 `ADD ESP,4` 같은 주소에 대한 개별 예외 대신 명령 호환성 정책을
적용했습니다. core probe와 Linux x64 실행 파일 빌드는 통과했습니다.

## English

Made x64 reentry use the common `CanResumeLinuxX64LegacyTarget` compatibility
decision before executing original bytes.

- Non-identical ordinary continuations clear TF/reentry state and fail closed
- Planner HLE entries dispatch directly through the shared guest HLE dispatcher
- `FF /2`, `FF /4`, `RET`, and branch-shaped boundaries use existing transfer handlers
- An unhandled non-identical boundary is refused when no cache target resolves

This applies an instruction-compatibility policy instead of an exception for an
individual address such as `ADD ESP,4`. The core probe and Linux x64 runtime
build passed.
