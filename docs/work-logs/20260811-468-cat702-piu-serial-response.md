# 20260811-468 공용 동적 포트 출력 래퍼 보존 작업 로그 / Shared Dynamic Port Output Wrapper Preservation Work Log

- 설계: [20260811-468-cat702-piu-serial-response](../design/20260811-468-cat702-piu-serial-response.md)
- 작업 지시: [20260811-468-cat702-piu-serial-response](../work-orders/20260811-468-cat702-piu-serial-response.md)
- 분석: [cat702-piu-lock-check](../analysis/cat702-piu-lock-check.md)

## 한국어

### 수행 결과

- 실제 `pumpitpc` transform/challenge/response와 게스트 후처리를 PIU10 probe에 추가했습니다.
- 기존 CAT702 HLE 모델이 예상 응답 `7D 77 FE EA 8B 7A 55 7D 5F 1E`와 일치함을
  확인했습니다.
- 무시·유예 `OUT DX,*` 출력에서 게스트 코드를 NOP 패치하던 동작을 제거하고 현재
  EIP만 전진하도록 수정했습니다.
- 조사 중 시도한 word-OUT AOT 분류 및 전용 wrapper 우회 변경은 최종 원인과 무관하여
  모두 제거했습니다.
- 공용 정책과 확정된 바이너리 분석 결과를 아키텍처·설계·분석 문서에 반영했습니다.

### 검증

- `cmd /c scripts\build_win32_x86.bat`: 성공
- `repiu_aot_probe.exe --piu10`: 성공,
  `piu10_cat702_vector=true,response=7d:77:fe:ea:8b:7a:55:7d:5f:1e`
- `repiu_aot_probe.exe build/runtime_mounts/pumpitpc/PIU/PIU.EXE`: exit code 0,
  전체 probe 성공
- 기본 환경 `repiu.exe pumpitpc`: 과거 약 9초에 발생하던 `INT 21h AH=08` 오류 종료
  없이 50초 이상 계속 실행됨. 검증 후 프로세스를 수동 종료했습니다.
- `git diff --check`: 성공

### 결론

Lock Error의 원인은 CAT702 구현이 아니라 공용 동적 포트 출력 래퍼의 파괴적 패치였습니다.
이번 수정은 특정 게임 바이너리 예외 없이 모든 DX 기반 출력 래퍼를 보존합니다.

## English

### Result

- Added the real `pumpitpc` transform/challenge/response and guest post-processing to the PIU10 probe.
- Confirmed that the existing CAT702 HLE model matches expected response
  `7D 77 FE EA 8B 7A 55 7D 5F 1E`.
- Removed guest-code NOP patching for ignored and deferred `OUT DX,*` accesses, advancing only
  the current EIP instead.
- Removed the exploratory word-OUT AOT classification and dedicated wrapper bypass because they
  were unrelated to the final cause.
- Recorded the shared policy and confirmed binary findings in architecture, design, and analysis docs.

### Verification

- `cmd /c scripts\build_win32_x86.bat`: passed
- `repiu_aot_probe.exe --piu10`: passed with
  `piu10_cat702_vector=true,response=7d:77:fe:ea:8b:7a:55:7d:5f:1e`
- `repiu_aot_probe.exe build/runtime_mounts/pumpitpc/PIU/PIU.EXE`: exit code 0, full probe passed
- Default `repiu.exe pumpitpc`: continued for more than 50 seconds without the previous
  `INT 21h AH=08` termination at about nine seconds; the process was stopped manually afterward.
- `git diff --check`: passed

### Conclusion

The Lock Error was caused by destructive patching of a shared dynamic port-output wrapper, not
the CAT702 implementation. The fix preserves every DX-based output wrapper without a game-binary exception.
