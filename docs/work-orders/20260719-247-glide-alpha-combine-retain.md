# Task 247 작업 지시: grAlphaCombine 미지원 식 유지 정책 적용

## 배경

Task 246 채증으로 zero-EIP(0x0304ED35)의 근인이 확정되었다:
`_GRALPHACOMBINE@20(3,1,0,1,0)`이 GLSL 번역기의 "unsupported Glide alpha-combine
equation" 실패로 게이트 미처리가 되고, 미처리 예외가 Task 233의 AOT 스택 스캔
복구를 통해 반환 주소로 ESP 미조정 점프하여 stdcall 24바이트(ret+args)를
누수시킨다. 이후 epilogue가 인자를 레지스터로 pop하고 RET이 0을 pop한다.

## 작업 항목

1. `_GRALPHACOMBINE@20` 핸들러에 color-combine과 동일한 유지 정책 적용
   (design 237 확장): unsupported 메시지는 상태 유지 후 정상 stdcall 반환,
   그 외 실패는 기존대로 경계 실패.
2. Win32 x86 Debug 빌드 후 `aot-dynamic` 180초 구동으로 검증:
   - 기존 ~75초 zero-EIP 종료 소멸.
   - Glide gate entries == handled (미처리 0).
   - progress가 90K를 넘어 다음 frontier 도달.
3. frontier 문서·작업 로그 갱신.

# Task 247 Work Order: Retain Unsupported grAlphaCombine Equations

Task 246 evidence pinned the zero-EIP root cause: an unsupported-equation failure
of `_GRALPHACOMBINE@20` leaves the gate unhandled, and the unhandled exception
reaches the Task 233 AOT stack-scan recovery, which jumps to the return address
without adjusting ESP, leaking the 24-byte stdcall frame. Apply the design-237
retain policy to the alpha-combine handler, build, and verify with a 180-second
`aot-dynamic` run: the ~75 s zero-EIP termination disappears, gate entries equal
handled, and progress advances past 90K to the next frontier.
