# 작업 지시: AOT 세그먼트 명령 번역
# Work Order: AOT Segment-Instruction Translation

**설계 (Design):** `docs/design/20260722-264-aot-segment-instruction-translation.md`
**Task:** 264 (Task 263 근인 대응)

## 목표 (Goal)

Task 263이 규명한 근인(세그먼트 명령 = AOT 이탈의 약 75%)을 줄이기 위해, 공용
planner/emitter에서 흔한 세그먼트 명령을 네이티브 번역한다. 동작은 바꾸지 않는다.

## 단계 (Phases)

* **선결 프로브(안전 계측):** push-seg 경계에서 host 세그먼트 vs shadow selector 비교로
  실제 semantics 확정. **완료** — host 항상 0x2b, 단일스텝은 push를 네이티브 실행.
* **Phase 1:** push-seg를 `IsHleBoundary`에서 면제 → `kCopy`(네이티브). **완료·검증.**
* **Phase 2:** `mov r/m,Sreg`(store) 동일 프로브 확인 후 kCopy. (미착수)
* **Phase 3:** 세그먼트 오버라이드 메모리 접근(GS 55%) — 단일스텝의 shadow-주소 해석
  여부 확인 후 prefix-strip/base-add. (미착수)

## 검증 (Verification)

VS 2026 Debug 빌드 + supervisor `pumpit1` aot-dynamic 실측. 각 단계:
`boundary_reason(other)` 감소, 크래시·fatal 0, 도달 EIP·Glide 렌더 동일(무회귀),
single_step 감소를 Task 263 카운터로 확인.

## 완료 조건 (Done)

* 각 Phase가 동작 무변경으로 해당 세그먼트 명령의 예외 경계를 제거.
* 설계·지시·로그·frontier 갱신.
