# AOT 반환 인라인 캐시 다중화 작업 로그
# Work Log: Multi-Entry AOT Return Inline Cache

## 1. 개요 (Overview)

Task 219가 확정한 디코드 루프 ~1000배 감속(반환 대상 교대에 의한 단일 엔트리 반환 인라인 캐시
스래싱)을 해소했다. 설계 문서(`docs/design/20260716-220-aot-return-inline-cache-two-way.md`)는
2엔트리로 시작했으나, 검증 과정에서 4엔트리 + 교체 정책으로 확장이 필요함이 확인되어 최종
구현은 4엔트리 직렬 체인이다.

## 2. 구현 경과 (Implementation Course)

### 2.1 1차: 2엔트리 (불충분으로 판명)

설계대로 반환 thunk에 비교/히트 블록을 하나 더 연결하고(entry0 우선 채움, entry1 교체) 40초
검증했으나 churn이 거의 그대로였다. 임시 진단(`icache patch` 샘플링 로그)으로 원인을 확정했다:
* 우리 RET(`0x030EE1DA`)의 thunk에서 entry0은 **네 번째 반환 대상 `0x030EE1F3`**(첫 호출부)에
  선점되어 있었고, 루프의 실제 순환 대상은 `0x030EE292`/`0x030EE300`/`0x030EE245` 3개였다.
* `repiu_aot_probe`로 헬퍼 `0x010EE170`을 직접 조회한 결과 정적 호출부는 **정확히 4곳**
  (`0x010EE1EE`/`0x010EE240`/`0x010EE28D`/`0x010EE2FB`)이었다.

### 2.2 2차: 4엔트리 배열 + 라운드로빈 교체 (최종)

1. `include/repiu/runtime/aot_code_cache.h`: ad-hoc `second_*` 필드 대신
   `AotInlineCacheEntry{compare/target_immediate/guard/jump_displacement_offset}` 배열과
   patcher 전용 `replace_cursor`를 `AotIndirectInlineCacheSite`에 추가. 간접 call/jmp
   사이트는 빈 배열로 기존 단일 슬롯 레이아웃 유지.
2. `src/runtime/aot_code_cache.cpp` `EmitReturnInlineCacheSlot`: 4엔트리 루프 방출.
   entry i의 초기 guard는 모두 `E9 → miss`, 패치 시 `0F 85`(JNE)로 교체되며 entry i의 JNE
   대상은 entry i+1의 compare, 마지막은 miss 꼬리. `pushfd` 1회/각 경로 `popfd` 복원으로
   EFLAGS 의미 보존. thunk 크기 27 → 약 99바이트.
3. `src/platform/win32/aot_code_cache_win32.cpp`:
   * `AppendWin32AotDynamicImage`: 동적 이미지 append 시 엔트리 배열 오프셋 재배치.
   * `PatchWin32AotIndirectInlineCache`: 대상 일치 엔트리 갱신 → 첫 빈 엔트리 채움 →
     `replace_cursor` 라운드로빈 교체 순의 stateless 정책. 샘플링 진단(`처음 16회 + 4096회마다`)
     유지.

## 3. 검증 결과 (Verification Results)

### 3.1 aot-dynamic 40초 (`task220-4way-40s.log`)

| 항목 | 수정 전 (Task 219 관측) | 수정 후 |
|---|---|---|
| `0x030EE1DA` boundary 고정 | 동결 구간 내내 고정 | **소멸** (bguest가 계속 이동) |
| `ret_dispatch` | 초당 ~820~1030 폭주 | 21초 누적 1,100회 |
| dispatch | 56857~56859에서 동결 | **58,158까지 전진** (디코드 통과) |
| 게스트 수명 | 90초+ 동결 지속 | 21초에 기지의 종료 지점 도달 |
| 로더 종료 | (이전 세대) post-attempt hang | **`child_exit=0` 정상 종료, hang 미재현** |

게스트 종료 지점은 guest `0x030873F4`의 `mov [ebx+ebp], al`(`88 04 2B`)이 `0x045D3EB0`에 쓰는
`0xC0000005` — Task 205/212가 확정한 디코드 출력 스토어와 동일 주소·동일 명령이다.

### 3.2 기본 trap 백엔드 30초 회귀 (`task220-trap-30s.log`)

progress 118,504 / single_step 794,936으로 완주. 기존 기준선(Task 208~209의 111~112k)과 동등
이상, fatal 없음 — 회귀 없음.

## 4. 결론 및 다음 단계 (Conclusion & Next Steps)

반환 인라인 캐시 4엔트리 다중화로 Task 216~219가 추적해온 "동결"이 해소되고 실행이 다음
frontier로 전진했다. 다음 frontier는 Task 212 미확정 1번의 재부상이다: Task 213의 resize 상한
모델링에도 불구하고 **정확히 같은 버퍼 주소 `0x045D3EB0`**이 재현되므로, 이 포인터의 출처는
resize 응답이 아니라 합성 DOS/4G client/private-data 풀 경계 값일 가능성이 높다. 디코드 구조체
`[ESI+0x34]`에 이 값을 채우는 코드의 역추적이 다음 작업이다.

## 5. 참고 (References)

* 로그: `task220-verify-40s.log`(2엔트리), `task220-diag-25s.log`(진단), `task220-4way-40s.log`
  (최종), `task220-trap-30s.log`(회귀) — 세션 스크래치패드
* 관련 문서: `docs/design/20260716-220-aot-return-inline-cache-two-way.md`,
  `docs/analysis/current-execution-frontier.md`(Task 219, 220 항목)
