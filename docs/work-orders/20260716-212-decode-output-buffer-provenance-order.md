# 작업 지시: 디코드 출력 버퍼 0x045D3EB0 provenance 진단 (Task 212)

설계: `docs/design/20260716-212-decode-output-buffer-provenance.md`
브랜치: `feature/212-decode-output-buffer-provenance`

## 작업 항목

1. `[x]` 설계 문서 작성 (정적 분석 결과 포함: fault 주소가 LINEXE private data 영역 내부, allocator 상한은 `client_data_base`)
2. `[x]` Phase A 진단 계측
   * `[x]` `CaptureException`(SEH 필터)에서 `ExceptionInformation` 접근 종류·fault VA 캡처
   * `[x]` fault VA의 `VirtualQuery` 결과(state/protect/region) 캡처
   * `[x]` `[ESI+0x20..+0x3C]` 8 dword checked 덤프
   * `[x]` attempt 필드·요약 로그 3종 추가
3. `[x]` 빌드 + aot-dynamic 180초 구동으로 진단 라인 회수
4. `[x]` 판별 완료: **arena-end overflow** — 실제 fault VA는 `0x045D7000`(arena 끝, MEM_FREE)이고 버퍼 base `0x045D3EB0`부터 `0x3150`바이트는 정상 기록됨. 버퍼 base는 LINEXE private data 영역 내부(충돌, 조용한 훼손). 분석 문서 갱신 완료.
5. `[ ]` 수정 방향 확정 — **후속 작업으로 분리**: (a) allocator 상한 정확 모델링(권장) vs (b) arena slack 확장. allocator heap 상한의 출처 역추적 선행 필요.
6. `[x]` 작업 로그 작성

## 검증 기준

* 빌드 통과, 진단 라인 3종이 종료 요약에 출력됨.
* 진단은 관찰 전용이므로 기존 180초 기준선과 동일 거동(창 생성, MSCDEX 처리, ~147초 게스트 종료).

---

# Work Order: Decode Output Buffer 0x045D3EB0 Provenance Diagnosis (Task 212)

Design: `docs/design/20260716-212-decode-output-buffer-provenance.md`. Branch: `feature/212-decode-output-buffer-provenance`.

Items: (1) design with static findings [done]; (2) Phase A instrumentation in the SEH filter — access kind/fault VA, `VirtualQuery` of the fault page, checked `[ESI+0x20..0x3C]` dump, attempt fields and three summary log lines [done]; (3) build and a 180-second aot-dynamic run to recover the diagnostics; (4) classify: uncommitted page vs LINEXE-region collision vs different address, updating the analysis docs; (5) settle the fix direction (this task or a follow-up) based on the evidence; (6) work log. Verification: build passes, the three diagnostic lines appear in the exit summary, and behavior matches the existing 180-second baseline (window, MSCDEX handled, guest death at ~147 s) since the diagnostics are observation-only.
