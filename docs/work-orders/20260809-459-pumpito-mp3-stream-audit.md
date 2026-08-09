# 20260809-459 pumpito MP3 stream 감사 작업 지시 / Pumpito MP3 Stream Audit Work Order

설계: [20260809-459-pumpito-mp3-stream-audit.md](../design/20260809-459-pumpito-mp3-stream-audit.md)

## 한국어

- [x] 사용자 로그에서 비정상 decode 비율과 첫 음악 손상을 확인합니다.
- [x] 환경변수로 제한된 원본 byte-loop 감사 상태를 구현합니다.
- [x] 실제 byte와 source cursor, frame count, `ECX` 비교 probe를 추가합니다.
- [x] Win32 x86 Debug build와 PIU10 probe를 실행합니다.
- [x] `pumpito`에서 최초 불일치와 교정 후 100구간 이상 통과를 확보합니다.
- [x] 확인된 guest 제어 경계를 보존하도록 batch 길이를 교정합니다.
- [x] 제한 실행에서 batch 활성화와 회복된 decode 밀도를 확인합니다.
- [x] 누적 분석, architecture와 작업 로그를 갱신하고 커밋합니다.
- [ ] 사용자 환경에서 첫 음악과 화면 동시 진행을 검증합니다.

## English

- [x] Confirm the abnormal decode density and damaged first track in the user's log.
- [x] Implement environment-gated original-byte-loop audit state.
- [x] Add probes for actual byte, source cursor, frame count, and `ECX` comparison.
- [x] Run the Win32 x86 Debug build and PIU10 probe.
- [x] Obtain the first mismatch and more than 100 passing segments after correction in `pumpito`.
- [x] Preserve the confirmed guest control boundary in the batch-length calculation.
- [x] Confirm batch activation and recovered decode density in a bounded run.
- [x] Update cumulative analysis, architecture, and the work log, then commit.
- [ ] Validate the first track and concurrent rendering in the user's environment.
