# Task 745: vsync 거부 시 swap 페이싱 작업 지시

설계: [20260927-745](../design/20260927-745-swap-pacing-when-vsync-refused.md)

## 한국어

1. `GlideSwapIntervalPolicySnapshot`에 실패 메시지·주사율·페이싱 상태·카운터를 더하고 최종 보고에
   찍는다.
2. `ResolveGlideSwapPacingPeriodMicroseconds(interval, refresh_hz)`를 두고 core probe로 검사한다.
3. 백엔드가 override 적용 실패(또는 effective 불일치) 시 페이싱을 켜고 `grBufferSwap` 뒤 다음 마감까지
   잔다.
4. WSLg에서 `REPIU_GLIDE_SWAP_INTERVAL=1`로 fps가 주사율에 잡히는지, override 없이는 그대로인지, Win32
   빌드·core probe를 확인한다.
5. 설계·작업 로그·README를 갱신하고 커밋한다.

## English

1. Add the failure text, refresh rate, pacing state and counters to `GlideSwapIntervalPolicySnapshot`
   and print them in the final report.
2. Add `ResolveGlideSwapPacingPeriodMicroseconds(interval, refresh_hz)` with a core-probe check.
3. Have the backend enable pacing when the override fails to apply (or the effective interval
   differs) and sleep until the next deadline after `grBufferSwap`.
4. Check on WSLg that `REPIU_GLIDE_SWAP_INTERVAL=1` pins the fps to the refresh rate, that nothing
   changes without an override, and that the Win32 build and core probes pass.
5. Update the design, work log and README, then commit.
