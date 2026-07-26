# 20260726-302 작업 로그: Glide 깊이 비교와 게이트 안전 반환 / Work log

설계: [20260726-302-glide-depth-compare-gate-safety.md](../design/20260726-302-glide-depth-compare-gate-safety.md)

작업 지시: [20260726-302-glide-depth-compare-gate-safety.md](../work-orders/20260726-302-glide-depth-compare-gate-safety.md)

## 한국어

### 결과

`grDepthBufferFunction`이 유효 Glide 비교 함수 `0..7`을 모두 OpenGL
`GL_NEVER..GL_ALWAYS`로 전달하도록 확장했습니다. 값 7도 depth test를 끄지 않으며,
기존처럼 `grDepthBufferMode`가 활성 여부를 담당합니다.

guest 반환 주소와 catalog signature를 검증한 뒤 사용할 공용 `decline_gate`를
추가했습니다. specialized handler의 후속 실패 25곳은 이제 bounded telemetry를 남기고,
non-void 반환은 `EAX=0`, EIP는 caller, ESP는 `4+argument_byte_count`만큼 정리한 뒤
정상 반환합니다. hard reject는 반환 주소 불량과 signature 불일치 두 곳만 남겼습니다.

### 원인 증거

사용자 로그에서 약 880초에 `_GRDEPTHBUFFERFUNCTION@4(3)`이 두 번 reject됐습니다.
다음 gate stack에는 depth-mask frame 아래에 `0x0304F5A5, 3`이 남았고, 최종 guest
명령은 `EAX=3`으로 `[eax+0x21F9] = 0x000021FC`를 읽어 `0xC0000005`를 냈습니다.
Tasks 246-248에서 확인한 미처리 Glide gate의 stdcall frame 누수와 같은 원인 사슬입니다.

### 구현

| 파일 | 변경 |
|---|---|
| `glide_opengl_backend.cpp` | depth compare `0..7` 범위 검증과 `GL_NEVER + function` 적용 |
| `linexe_glide_boundary.cpp` | ABI 검증 이후 공용 safe decline 및 25개 실패 경로 연결 |
| `ARCHITECTURE.md` | 비교 함수와 hard-reject/safe-decline 책임 기록 |
| `docs/design/`, `docs/analysis/` | 기존 제약 갱신, 사용자 로그 근인과 검증 결과 누적 |

### 검증

`scripts/build_win32_x86.bat`로 VS2026 Win32 x86 Debug 전체 빌드를 수행했습니다.
빌드는 성공했으며 기존 C4819 경고 외 compile/link 오류는 없었습니다.

새 loader를 `REPIU_EXECUTION_TIMEOUT_MS=30000`으로 실행했습니다.

| 항목 | 결과 |
|---|---:|
| process exit | 0 |
| 종료 이유 | 정상 30초 timeout |
| diagnostic progress | 542,996 |
| Glide gate entries/handled | 49/49 |
| `grDepthBufferFunction(7)` | 3회 성공 |
| gate reject / safe decline | 0 / 0 |
| OpenGL error / caught exception / fatal | 0 / 0 / 0 |
| hard `return reject_gate` 소스 위치 | 2개(반환 주소, signature) |

`git diff --check`도 통과했습니다.

### 제한

인자 3은 사용자 실행에서 약 880초에 처음 나타났습니다. 30초 smoke는 기존 인자 7의
초기 경로와 일반 회귀만 검증하며, 인자 3의 live 적용과 원래 종료 지점 통과는 다음
interactive 장시간 실행에서 최종 확인해야 합니다. 다만 값 3은 새 `0..7` 범위에 포함되고,
backend가 실패하더라도 검증된 stdcall frame은 공용 decline이 정리하므로 기존 두 종료
조건은 코드 구조상 제거됐습니다.

---

## English

### Result

Extended `grDepthBufferFunction` to map every valid Glide comparison `0..7` to
OpenGL `GL_NEVER..GL_ALWAYS`. Depth-test enablement remains owned by
`grDepthBufferMode`.

Added a common `decline_gate` after guest-return and catalog-signature
validation. Twenty-five specialized-handler failure sites now record bounded
telemetry, supply conservative `EAX=0` for non-void returns, restore the caller
EIP, clean `4 + argument_byte_count` bytes from ESP, and return normally. Only
invalid return addresses and signature mismatches remain hard rejects.

The user log reaches `_GRDEPTHBUFFERFUNCTION@4(3)` at about 880 seconds. Two
rejects leave `0x0304F5A5, 3` beneath the next gate frame, after which guest code
reads `EAX(3) + 0x21F9 = 0x21FC` and raises `0xC0000005`. This matches the
Tasks 246-248 unhandled-gate stdcall leak class.

The full VS2026 Win32 x86 Debug build succeeded with only existing C4819
warnings. A 30-second run ended by normal timeout with process exit 0, progress
542,996, 49/49 handled Glide gates, three successful compare-7 calls, and zero
gate reject, safe decline, OpenGL error, caught exception, or fatal. Source
inspection leaves exactly two hard rejects, both before ABI trust, and
`git diff --check` passes.

The observed compare-3 path first appears around 880 seconds, so crossing the
original terminal frontier remains a follow-up interactive long-run check. The
two code conditions that caused the failure are nevertheless removed: value 3
is valid in the new mapping, and any later post-signature backend failure cleans
the verified stdcall frame.
