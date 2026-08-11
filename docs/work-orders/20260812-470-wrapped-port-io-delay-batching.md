# Task 470 작업 지시 — 호출 래퍼형 포트 I/O 지연 루프 batching

## 한국어

1. `pumpitpc` 공용 포트 입력 래퍼와 호출자 모양을 런타임 census 및 정적 dump로 확인합니다.
2. `port_io_delay_loop`에 주소 독립적인 입력 래퍼 해석과 호출자 루프 검증을 추가합니다.
3. 기존 direct-loop matcher와 새 wrapped-loop matcher의 fail-closed probe를 추가합니다.
4. `pumpitpc` Release 실행으로 batch 활성화와 예외 감소를 측정합니다.
5. 확인한 사실을 포트 I/O 분석 문서와 작업 로그에 반영합니다.

구현은 JAMMA 입력에만 적용하며 guest EIP/EFLAGS, 입력 결과, 포트 레지스터를 합성하지
않습니다. 증명된 counter만 전진시키고 마지막 반복은 원본 guest 코드가 실행합니다.

## English

1. Confirm the shared `pumpitpc` input wrapper and caller shapes with runtime census and static
   dumps.
2. Add address-independent wrapper decoding and caller-loop validation to `port_io_delay_loop`.
3. Add fail-closed probes for the existing direct loop and the new wrapped loop.
4. Measure batch activation and exception reduction in a Release `pumpitpc` run.
5. Record confirmed findings in the port-I/O analysis and work log.

The implementation applies only to JAMMA input. It does not synthesize guest EIP/EFLAGS, the
input result, or the port register; it advances only a proven counter and lets original guest code
execute the final iteration.
