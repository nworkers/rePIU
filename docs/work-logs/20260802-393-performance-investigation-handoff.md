# 20260802-393 성능 조사 인계 및 병합 작업 로그 / Performance Investigation Handoff and Merge Work Log

설계: [20260802-393-performance-investigation-handoff.md](../design/20260802-393-performance-investigation-handoff.md)

작업 지시: [20260802-393-performance-investigation-handoff.md](../work-orders/20260802-393-performance-investigation-handoff.md)

## 한국어

### 문서화

- Task 377~392에서 채택한 기본 정책과 장시간 측정으로 기각한 segment-override broad/hybrid 정책을 종합했습니다.
- 구현의 공용 명령·policy 기반 범위와 profile metadata가 필요한 Glide 적용 범위를 구분했습니다.
- 다음 확인 항목을 개별 `8A/88/89/8C/8E` 형식, 다른 guest selector 비율, 동일 작업량 재귀속, `pumpit1` 재현 조건으로 정리했습니다.
- 무음 보고가 hybrid가 아니라 `pumpit1` 인자 누락에 따른 CHD/MSCDEX 미마운트였음을 기록했습니다.

### 검증

- Release `repiu_loader_win32`와 `repiu_aot_probe` 빌드가 성공했습니다. 기존 C4819와 Zydis LNK4217 경고만 남았습니다.
- `MASTER/PIU_1ST/PIU/PIU.EXE`와 `build/runtime_mounts/pumpit1/PIU/PIU.EXE` 전체 probe가 모두 종료 코드 0입니다.
- 양쪽에서 `segment_override_hybrid_patch=true`, `selector_guard_all=true`, `coherence_all=true`를 확인했습니다. `cache_executable=false`는 해당 probe의 의도된 데이터 배치 검사 결과입니다.

### 병합

- `VERSION` patch를 `0.0.121`에서 `0.0.122`로 증가시켰습니다.
- 작업 브랜치 전체를 하나의 최종 커밋으로 squash하여 `main`에 병합하고 로컬 annotated tag `v0.0.122`를 생성하는 절차를 수행했습니다.
- 원격 push는 수행하지 않습니다.

## English

### Documentation

- Consolidated the default policies adopted in Tasks 377-392 and the long-run rejection of broad/hybrid segment-override routing.
- Distinguished shared instruction/policy-based mechanisms from Glide applicability that requires profile metadata.
- Recorded next checks as individual `8A/88/89/8C/8E` forms, selector rates on other guests, matched-work re-attribution, and `pumpit1` reproduction conditions.
- Recorded that the silent-audio report came from omitted `pumpit1` and absent CHD/MSCDEX, not hybrid routing.

### Verification

- Release builds of `repiu_loader_win32` and `repiu_aot_probe` passed with only pre-existing C4819 and Zydis LNK4217 warnings.
- Full probes against `MASTER/PIU_1ST/PIU/PIU.EXE` and `build/runtime_mounts/pumpit1/PIU/PIU.EXE` both exited zero.
- Both reported `segment_override_hybrid_patch=true`, `selector_guard_all=true`, and `coherence_all=true`. `cache_executable=false` is the intentional data-placement probe result.

### Merge

- Bumped the patch version from `0.0.121` to `0.0.122`.
- Performed the requested squash merge of the complete task branch into `main` and created local annotated tag `v0.0.122`.
- Do not push remotely.
