# Task 692 — mode16 PUSH의 SS 주소 및 폭 수정

SS descriptor 기반 공용 push access 계산과 전용 mode16 adapter를 추가했습니다.
word/dword operand와 SS.B를 분리하고 저장 성공 후에만 ESP/EIP를 변경합니다.
실제 메모리 probe에서 PUSH AX/EDI/SP/ESP/CS, 주변 byte 보존, flags 보존,
쓰기 거부 시 상태 보존을 검증했습니다. 주소별 예외는 없습니다.

Linux `repiu_core_probe`와 `repiu` 빌드가 성공했습니다. build에 기존 extern
경고와 WSL/Windows clock-skew 경고가 있었으나 변경된 object의 컴파일과
실행 파일 링크를 확인했습니다.

```text
segment_push_stack_geometry=true,word=1,dword=1,wrap=1,big_word=1,big_dword=1
mode16_push_writes=true
core_probe_total=27
core_probe_failures=0
core_probe_all=true
```

실행 trace로 동적 SS의 base=0158A83C, limit=FFFF, flags=0092를 확인했습니다.
기존 PUSH CS는 01581FFC에 dword를 기록했으며 올바른 word 주소는 0158C83A입니다.
Task 690/691 모두 stop=0110002E였다는 정정을 각 기록에 추가했습니다.

수정 후 `repiu pumpit2a`는 PUSH CS, PUSH AX, `66 57` (PUSH EDI)를 통과하고
01100031의 bare RETF에서 SIGTRAP으로 중단했습니다. ESP=01581FF8은 세 PUSH의
2+2+4 byte 감소와 일치합니다. 기존 fault 로그의 guest_stack 필드는 ESP를
선형 주소로 읽으므로 non-flat SS의 실제 stack 내용으로 해석할 수 없습니다.
첫 filtered 실행은 출력 없이 종료되어 근거로 사용하지 않았고 재실행의 전체
종료 로그로 위 결과를 확인했습니다. 정상 게임 실행이나 coredump 해결은 아닙니다.

다음 작업: SS 기반 frame read와 bare mode16 RETF를 설계하고 실제 target
selector를 검증합니다. 현재 handler는 `66 CB`의 dword frame만 처리합니다.

## English

Added shared descriptor-based push access planning and a dedicated mode16 adapter.
Operand width and SS.B are independent; ESP/EIP update only after a successful
write. Real-memory probes verify AX/EDI/SP/ESP/CS pushes, neighboring bytes,
flags and rejection without state changes. There are no address-specific rules.

Linux core probe and repiu builds passed. Existing extern and WSL/Windows clock
skew warnings appeared; changed objects compiled and binaries linked. All 27
core probe groups passed, including stack geometry and real writes shown above.

Tracing confirms dynamic SS base=0158A83C, limit=FFFF, flags=0092. The previous
PUSH CS stored a dword at 01581FFC instead of a word at 0158C83A. Corrected the
Task 690/691 interpretation: both already stopped at 0110002E.

After this change pumpit2a passes PUSH CS, PUSH AX and 66 57 (PUSH EDI), then
stops with SIGTRAP at bare RETF, 01100031. ESP=01581FF8 matches 2+2+4 bytes.
The existing fault dump reads ESP as linear, so its stack fields are not the
actual non-flat SS stack. The first filtered run produced no evidence; a rerun's
full exit log established this result. Successful game execution and resolution
of the core-dump termination remain unverified.

Next: design SS-relative frame reading for bare mode16 RETF and validate the
actual target selector. The current handler only accepts the 66 CB dword frame.
