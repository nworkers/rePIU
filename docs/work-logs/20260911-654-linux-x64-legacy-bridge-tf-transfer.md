# 20260911-654 작업 로그: Linux x64 legacy bridge TF 전이

설계: [20260911-654 설계](../design/20260911-654-linux-x64-legacy-bridge-tf-transfer.md)  
작업 지시: [20260911-654 작업 지시](../work-orders/20260911-654-linux-x64-legacy-bridge-tf-transfer.md)  
분석: [linux-port-frontier 3.91](../analysis/linux-port-frontier.md)

## 한국어

### 결과

1. Task 653 thunk가 TF 설정 뒤 host `MOV`를 실행하던 순서를 교정했습니다.
2. guest continuation과 EAX를 먼저 복원하고, scratch register로 guest EFLAGS에 TF를
   더한 뒤 thunk를 `POPFQ; JMP guest`로 끝내도록 변경했습니다.
3. 실제 실행에서 bridge 뒤 첫 guest 경계 `0x010F925F`와 direct CALL
   `0x010F9273`의 single-step을 확인했습니다.
4. `0x010F9273` HLE가 guest `[0x0158CC50]`에 `0x010F9278`을 기록했고 두 번째
   `0x010F1E56` RET가 그 주소로 복귀했습니다. Task 653의 zero-return frontier는
   해소됐습니다.

### 검증

* Linux x64 Debug `repiu`와 `repiu_core_probe` 빌드: 통과
* 전체 core probe: `27/27`, failures `0`
* 실제 watch: `0x010F925F`, `0x010F9273` 각각 single-step 1회
* 실제 guest write: `0x010F9273 -> [0x0158CC50] = 0x010F9278`
* 실제 allocator RET: `0x010F1E56 -> 0x010F9278`

### 남은 경계

게임은 아직 정상 실행되지 않습니다. 새 frontier는 `0x010F927C`의
`A3 98 66 1A 01` (`MOV [0x011A6698],EAX`)입니다. 원본 bytes는 long mode에서
주소 operand 폭이 달라져 SIGSEGV를 냅니다. 다음 작업은 이 moffs32 store의 공용 HLE
또는 안전한 lowering 경계를 설계하고 검증하는 것입니다.

## English

### Result

1. Corrected the Task 653 thunk ordering that executed a host `MOV` after
   activating TF.
2. The thunk now restores the guest continuation and EAX first, adds TF to
   guest EFLAGS in a scratch register, and ends with `POPFQ; JMP guest`.
3. The real run observed single-step at the first post-bridge guest boundary
   `0x010F925F` and at the direct CALL `0x010F9273`.
4. HLE for `0x010F9273` wrote `0x010F9278` to guest `[0x0158CC50]`, and the
   second `0x010F1E56` RET returned to that address. Task 653's zero-return
   frontier is resolved.

### Verification

* Linux x64 Debug `repiu` and `repiu_core_probe` build: passed
* Complete core probe: 27/27, zero failures
* Real watches: one single-step each at `0x010F925F` and `0x010F9273`
* Real guest write: `0x010F9273 -> [0x0158CC50] = 0x010F9278`
* Real allocator RET: `0x010F1E56 -> 0x010F9278`

### Remaining frontier

The game still does not run normally. The new frontier is
`A3 98 66 1A 01` (`MOV [0x011A6698],EAX`) at `0x010F927C`. The original bytes
have a different address-operand width in long mode and raise SIGSEGV. The next
task must design and verify shared HLE or another safe lowering boundary for
this moffs32 store.
