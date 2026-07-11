# DLL loader INT 21h AX=FF00h 역추적 작업 지시

1. `+0xF3438` 주변 원본 명령과 진입 경로를 복원한다.
2. 필터링한 장시간 실행으로 fatal 분기 식별 레지스터를 수집한다.
3. 선택된 초기화 실패 함수와 전역 selector provenance를 추적한다.
4. 현재 HLE 계약과 원본의 기대 계약 차이를 문서화한다.
5. 다음 구현 의사결정에 필요한 미확정 항목을 분리한다.

# DLL Loader INT 21h AX=FF00h Provenance Work Order

Recover the original instructions and entry path around `+0xF3438`, collect the fatal-branch discriminator from a filtered extended run, trace the selected initialization failure and selector provenance, document the mismatch between the current HLE response and the original contract, and isolate unresolved inputs needed for the next implementation decision.
