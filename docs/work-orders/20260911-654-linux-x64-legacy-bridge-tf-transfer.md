# 20260911-654 작업 지시: Linux x64 legacy bridge TF 전이

설계: [20260911-654 설계](../design/20260911-654-linux-x64-legacy-bridge-tf-transfer.md)

## 한국어

1. legacy-resume thunk의 register/flag 복원 순서를 `POPFQ; JMP guest`가 마지막 연속
   명령이 되도록 변경합니다.
2. resolver admission 및 fail-closed 정책은 유지합니다.
3. Linux x64 Debug 빌드와 전체 core probe를 실행합니다.
4. 실제 `pumpit2a` watch와 stack trace로 guest #DB 재진입, `0x010F9273` CALL의 guest
   반환 주소, 두 번째 allocator RET를 검증합니다.
5. 아키텍처·분석·작업 로그를 갱신하고 커밋합니다.

## English

1. Reorder register/flag restoration in the legacy-resume thunk so that
   `POPFQ; JMP guest` are its final consecutive instructions.
2. Preserve resolver admission and fail-closed policy.
3. Build Linux x64 Debug and run the complete core probe.
4. Verify guest #DB reentry, the guest return address for the `0x010F9273`
   CALL, and the second allocator RET with real `pumpit2a` watches and stack
   tracing.
5. Update architecture, analysis, and the work log, then commit.
