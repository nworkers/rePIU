# Task 247/248 작업 로그: 미처리 Glide 게이트 프레임 누수 수정 (combine/blend 유지 정책 + 프레임 루프 게이트)

## 수행 내용

Task 246이 확정한 손상 사슬(미처리 게이트 → Task 233 AOT 스택 스캔 복구가 ESP
미조정으로 반환 주소 점프 → stdcall 프레임 누수 → 이후 epilogue가 인자를 pop →
RET이 0/쓰레기 pop)에 대해, 관측된 미처리 게이트를 순차적으로 제거했다.

1. **Task 247**: `_GRALPHACOMBINE@20`에 design 237의 유지 정책 적용 —
   "unsupported Glide alpha-combine equation" 거부는 상태 유지 후 정상 stdcall
   반환. (근인 사슬의 첫 관측 사례: ret=0x0304ECBB 2회 미처리 → 24바이트 누수 →
   0x0304ED35 RET zero-EIP.)
2. **Task 248**: 프레임 루프 게이트 시그니처/핸들러 추가 —
   `_GRBUFFERCLEAR@12`(no-op), `_GRBUFFERSWAP@4`(no-op),
   `_GRBUFFERNUMPENDING@0`(EAX=0). 렌더링 충실도는 기존 그리기 no-op과 같은
   렌더링 경계 정책으로 후속 backend 과제에 위임.
3. **Task 248 추가**: `_GRALPHABLENDFUNCTION@16`의 "unsupported Glide
   alpha-blend function" 거부에도 동일 유지 정책 적용(관측: ret=0x0304F49C 2회
   미처리 → `ret 0xC` epilogue AV).

## 검증 (aot-dynamic 180초 구동 연쇄)

| 구동 | 게이트 entries/handled | 종료 | progress |
|---|---|---|---|
| Task 246 채증 | 61/59 (alphaCombine 2회 미처리) | ~74.7s EIP=0 fail-closed | 90,495 |
| Task 247 수정 후 | 73/71 (bufferClear 2회 미처리) | ~74s ret 0xC epilogue AV | 90,501 |
| Task 248 1차 후 | 84/82 (alphaBlend 2회 미처리) | ~76s epilogue AV | 93,760 |
| Task 248 최종 | 96+ / reject 0 | **180초 완주(타임아웃, fatal 없음)** | **131,803** |

각 수정마다 미처리 게이트가 정확히 다음 관측 지점으로 이동하며 게이트 트래픽이
전진(61→73→84)했다 — 미처리 게이트 = 프레임 누수 = 다음 크래시라는 사슬이 세 번
연속 재확인되었다.

**최종 구동(`scratch_run248b.log`)은 aot-dynamic으로 180초를 fatal 없이
완주(타임아웃 종료)했고, 게이트 로그에 메인 렌더 프레임 루프가 나타났다:
`grBufferClear → grColorMask → grBufferSwap → grBufferNumPending` 반복.
progress 131,803(Task 243 가드 시절 22,920의 5.75배), 종료 시점까지 지속 상승.**

## 남은 구조적 과제

1. 미계측 reject 지점 다수(각 핸들러의 backend 실패 `return false`)가 남아 있어,
   새 미처리 게이트가 나올 때마다 게이트 전수 로그로 식별해야 한다. 전 지점을
   `reject_gate`로 전환하는 정리가 필요하다.
2. 근본적으로, **미처리 게이트 예외가 Task 233의 AOT 스택 스캔 복구로 흘러들어
   ABI를 훼손하는 경로 자체**가 위험하다(Task 243의 스택 검색 금지 원칙과 동일
   계열). 게이트 주소에서 이 복구를 차단(fail-closed)하는 설계가 후속 과제다.

# Task 247/248 Work Log: Unhandled Glide Gate Frame-Leak Fixes

Applied the design-237 retain policy to `_GRALPHACOMBINE@20` (Task 247) and
`_GRALPHABLENDFUNCTION@16`, and added ABI-preserving frame-loop gates
grBufferClear/Swap/NumPending (Task 248), eliminating the observed instances of
the Task 246 corruption chain (unhandled gate → AOT stack-scan recovery jumps to
the return address without adjusting ESP → stdcall frame leak → epilogue pops
arguments → RET pops zero/garbage). Successive 180-second runs advanced gate
traffic 61→73→84 with the unhandled pair moving to the next observed gate each
time, reconfirming the chain three times. Remaining structural work: convert all
handler-internal `return false` sites to the logged reject path, and design a
fail-closed block so unhandled gate exceptions can never reach the Task 233
stack-scan recovery.
