# LINEXE 런타임 추출 작업 지시

1. DOS/16M BW module/segment/relocation 공용 모델을 정의합니다.
2. MZ declared end부터 BW chain을 안전하게 순회합니다.
3. `LINEXE.EXP`의 selector images와 RSI-2 relocation을 복원합니다.
4. asset `DOS4GW.EXE`에서 추출 결과와 기존 분석값을 비교합니다.
5. Win32 x86 빌드로 검증하고 guest 배치에 필요한 다음 결정을 기록합니다.

# Runtime LINEXE Extraction Work Order

Define the shared BW model, safely walk the bound chain, reconstruct and relocate `LINEXE.EXP`, compare runtime extraction with recovered evidence, build Win32 x86, and record the next guest-placement decision.
