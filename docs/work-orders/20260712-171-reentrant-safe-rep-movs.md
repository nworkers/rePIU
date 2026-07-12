# 재진입 안전 REP MOVS 작업 지시 / Work Order

## 한국어

1. host guest-range copy helper를 추가한다.
2. normal-memory destination을 사용하는 REP MOVS copy를 실패 반환형 API로 교체한다.
3. Win32 x86 Debug 빌드를 검증한다.
4. 장기 실행에서 기존 340초 access violation 재발 여부를 확인한다.
5. 분석과 작업 로그를 갱신한다.

## English

1. Add a host guest-range copy helper.
2. Replace REP MOVS copies to normal memory with failure-returning APIs.
3. Verify the Win32 x86 Debug build.
4. Check whether the former 340-second access violation recurs.
5. Update analysis and the work log.
