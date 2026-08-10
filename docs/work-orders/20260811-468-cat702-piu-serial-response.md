# 20260811-468 공용 동적 포트 출력 래퍼 보존 작업 지시 / Shared Dynamic Port Output Wrapper Preservation Work Order

설계: [20260811-468-cat702-piu-serial-response](../design/20260811-468-cat702-piu-serial-response.md)

## 한국어

### 목표

무시 대상 포트 출력이 공용 `OUT DX,*` 래퍼를 영구 변경하지 않게 하여,
`pumpitpc`의 후속 PIU10 CAT702 직렬 통신을 보존합니다.

### 작업 항목

- [x] `pumpitpc` Lock Error 경로와 CAT702 호출 경로를 추적합니다.
- [x] 실제 CAT702 transform/challenge/response 벡터로 HLE 모델을 검증합니다.
- [x] 공용 `OUT DX,AX` 래퍼가 초기 출력에서 NOP 처리되는 원인을 확인합니다.
- [x] 무시·유예 출력은 코드 패치 대신 EIP만 전진하도록 수정합니다.
- [x] 특정 target, 주소, 포트에 의존하지 않는 공용 정책으로 구현합니다.
- [x] Win32 x86 Debug 빌드와 전체 probe를 수행합니다.
- [x] 기본 `pumpitpc` 실행이 Lock Error 대기 경로를 벗어나는지 확인합니다.
- [x] 분석 문서와 작업 로그를 완료하고 커밋합니다.

## English

### Objective

Preserve subsequent PIU10 CAT702 serial communication in `pumpitpc` by ensuring that ignored
port outputs do not permanently modify a shared `OUT DX,*` wrapper.

### Work Items

- [x] Trace the `pumpitpc` Lock Error and CAT702 call paths.
- [x] Verify the HLE model with the real CAT702 transform/challenge/response vector.
- [x] Confirm that an initial output NOP-patches the shared `OUT DX,AX` wrapper.
- [x] Advance EIP instead of patching code for ignored and deferred outputs.
- [x] Implement a shared policy without target, address, or port-specific conditions.
- [x] Run the Win32 x86 Debug build and complete probe.
- [x] Confirm that default `pumpitpc` execution leaves the Lock Error wait path.
- [x] Complete analysis/work-log documentation and commit.
