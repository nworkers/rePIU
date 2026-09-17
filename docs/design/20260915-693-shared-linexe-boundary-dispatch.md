# Task 693 — 공용 far-jump dispatch의 LINEXE HLE 우선순위

Task 692의 RETF target 0080:1B28은 기존 export ABI 문서의 LINEXE_LOADMODULE입니다.
기존 `HandleLinexeFarTransferBoundary`는 fault boundary에서 호출되지만 AOT가
guest EIP를 복원한 공용 dispatch에서는 호출되지 않습니다. 공용 EA 처리에서
기존 LINEXE handler를 먼저 호출하고, 처리하지 않은 경우 일반 far jump를
계속합니다. 서비스 의미나 matcher를 새로 만들지 않습니다.

handler의 bridge frame을 읽을 수 없으면 저장된 이전 frame을 소비하지 않도록
즉시 거부합니다. probe는 공용 dispatch를 통해 알려진 LOADMODULE의 결과와
ESP/register 복원, 미지원 export의 일반 far-jump fallback을 확인합니다.
Linux 빌드와 실제 trace로 기존 HLE 진입과 새 실행 경계를 확인합니다.

## English

Task 692's RETF target 0080:1B28 is the documented LINEXE_LOADMODULE export.
The existing LINEXE handler is called at fault boundaries but not by shared
dispatch after AOT restores guest EIP. Try it first in shared EA dispatch;
continue generic far-jump handling when it declines. Reuse existing service
semantics and matching.

Reject unreadable bridge frames immediately instead of consuming stale saved
frames. Probe known LOADMODULE through shared dispatch, including register/ESP
restoration, and generic far-jump fallback for an unknown export. Build on Linux
and trace live HLE entry and the resulting execution frontier.
