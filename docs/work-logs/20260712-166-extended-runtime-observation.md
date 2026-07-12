# PIU 장기 실행 관찰 작업 로그 / Work Log

## 한국어

PIU 실행 제한을 120초로 늘렸습니다. 기존 `Not PTX file`과 `exit(-1)`은 재발하지 않았고 heartbeat, dispatch, progress가 전체 관찰 시간 동안 계속 증가했습니다. 실행은 자체 정상 종료하지 않았으며 관찰 제한으로 종료됐습니다.

23초 이후 EIP가 주로 object 2 `+0xDE1xx`에 집중됨을 확인하고 원본 바이트를 대조했습니다. 해당 구간은 bit 단위 unpack/decode 계산 루프입니다. 실행 trampoline이 모든 명령 뒤 Trap Flag를 재설정하므로 약 1,361만 guest 명령을 처리하는 데 120초가 걸립니다. 다음 구현 의사결정은 guest 코드를 안전하게 묶음 실행할 native fast path의 경계 방식입니다.

## English

Extended the PIU execution limit to 120 seconds. The former `Not PTX file` and `exit(-1)` did not recur, while heartbeat, dispatch, and progress increased throughout the observation. PIU did not exit on its own; the observation limit ended the run.

EIP samples after 23 seconds concentrated at object 2 `+0xDE1xx`; comparison with original bytes identifies a bit-oriented unpack/decode loop. Because the trampoline resets Trap Flag after every instruction, processing roughly 13.61 million guest instructions takes 120 seconds. The next implementation decision is how to define a safe native fast-path boundary for batched guest execution.
