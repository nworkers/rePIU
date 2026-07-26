# 20260726-302 작업 지시: Glide 깊이 비교와 게이트 안전 반환 / Work order

설계: [20260726-302-glide-depth-compare-gate-safety.md](../design/20260726-302-glide-depth-compare-gate-safety.md)

## 한국어

### 목표

`grDepthBufferFunction(3)`을 포함한 유효 비교 함수 `0..7`을 실제 OpenGL 상태로
반영하고, ABI가 검증된 Glide handler 실패가 stdcall frame을 누수시키지 않도록
공용 안전 반환 경로를 적용합니다.

### 구현 순서

1. `SetDepthBufferFunction`의 단일 값 제한을 `0..7` 범위 매핑으로 교체합니다.
2. Glide gate의 반환 주소와 signature 검증 뒤 사용할 `decline_gate` helper를
   추가합니다.
3. specialized handler의 검증 이후 실패 경로를 안전 반환 helper로 연결합니다.
4. 기존 hard reject가 잘못된 반환 주소와 signature 불일치에만 남는지 확인합니다.
5. `ARCHITECTURE.md`, 기존 Glide 설계와 누적 analysis를 갱신합니다.
6. Win32 x86 Debug 빌드와 가능한 smoke run을 수행합니다.
7. 검증 결과와 장시간 재현 제한을 작업 로그에 기록하고 커밋합니다.

### 완료 조건

- depth comparison 값 `0..7`이 backend에서 지원됩니다.
- `grDepthBufferFunction(3)`이 gate reject로 떨어지지 않습니다.
- ABI 검증 이후 specialized handler 실패는 EIP/ESP를 정상 정리합니다.
- Win32 x86 Debug 빌드가 성공합니다.
- smoke run에서 새 Glide reject, OpenGL 오류, caught exception이 없습니다.

---

## English

### Objective and steps

Support every valid depth comparison value `0..7`, including the observed
`grDepthBufferFunction(3)`, and ensure that a specialized Glide handler cannot
leak a validated stdcall frame. Add a common post-signature decline helper,
route specialized-handler failures through it, retain hard rejection only for
untrusted return-address or signature failures, update the related documents,
then build Win32 x86 Debug and run available smoke coverage.

Completion requires valid depth comparison mapping, normal EIP/ESP cleanup
after post-signature declines, a successful Win32 x86 Debug build, and no new
Glide reject, OpenGL error, or caught exception in the smoke run.
