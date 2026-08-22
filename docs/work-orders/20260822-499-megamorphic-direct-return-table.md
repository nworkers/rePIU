# Megamorphic direct-return table 작업 지시

설계: [20260822-499-megamorphic-direct-return-table.md](../design/20260822-499-megamorphic-direct-return-table.md)

측정 절차: [return stage 귀속 가이드](../guides/return-stage-attribution.md)

1. 평면 memo table 자료구조와 opt-in 환경 변수 정책을 플랫폼 공용 파일로 추가합니다.
   항목은 `{guest_key, cache_target}` 8바이트, 크기는 2의 거듭제곱, 기본 13비트입니다.
2. return inline cache emitter가 `REPIU_AOT_DIRECT_RETURN_TABLE`이 켜진 빌드에서만
   `miss_cache_offset` 앞에 probe를 emit하고, table 절대 주소와 mask fixup을 site 목록에
   기록합니다. 꺼져 있으면 emit 자체를 하지 않아 캐시 바이트가 지금과 같아야 합니다.
3. 배치와 동적 append가 fixup을 절대 주소로 패치하고 offset을 재보정합니다.
   `timer_safe_point_sites`와 같은 규약을 씁니다.
4. `HandleAotReturnTransfer`가 `ResolveAotTransferTarget` 성공 후 활성 대응(kActiveHit)만
   table에 기록합니다. Glide gate direct target과 dynamic translation 결과는 넣지
   않습니다.
5. `RetireWin32AotGuestPage`의 guard reset 지점에서 table 전체를 지웁니다.
6. 적중·삽입·무효화 카운터를 실행 요약에 연결합니다.
7. 합성 probe를 추가합니다: 해시와 mask, 삽입과 덮어쓰기, 빈 슬롯 규약, 무효화,
   emit 켜짐/꺼짐에 따른 바이트 동일성, fixup 재보정, 그리고 **생성된 probe 시퀀스를
   실제로 실행해** 적중 시 EIP와 ESP가 원본 RET과 같은지 확인합니다.
8. Win32 x86 Debug/Release로 `repiu_aot_probe`, `repiu`를 빌드하고 pumpit8 전체 probe를
   실행합니다.

## 완료 조건

계측 off에서 캐시 바이트와 기존 로그가 그대로여야 합니다. 켠 실행은 return fallback 0,
`scans=0`을 유지하고, 적중 경로가 원본 RET과 동일한 EIP·ESP 효과를 내는 것이 probe로
확인되어야 합니다. 성능 판정은 이 작업에 포함하지 않습니다 — 같은 구간 3회 재현이
필요하므로 사용자 실행으로 넘깁니다.

---

# Megamorphic Direct-Return Table Work Order

Design: [20260822-499-megamorphic-direct-return-table.md](../design/20260822-499-megamorphic-direct-return-table.md)

Procedure: [return-stage attribution guide](../guides/return-stage-attribution.md)

1. Add the flat memo table and its opt-in environment policy in a platform-neutral file:
   eight-byte `{guest_key, cache_target}` entries, a power-of-two size, thirteen bits by default.
2. Emit the probe ahead of `miss_cache_offset` in the return inline-cache emitter only when
   `REPIU_AOT_DIRECT_RETURN_TABLE` is on, recording the table-address and mask fixups on the
   site. With it off, nothing is emitted and the cache bytes must be identical to today.
3. Patch those fixups to absolute addresses at placement and re-offset them on dynamic append,
   following the `timer_safe_point_sites` convention.
4. Record only active-hit resolutions from `HandleAotReturnTransfer` after
   `ResolveAotTransferTarget` succeeds; exclude Glide-gate direct targets and dynamic-translation
   results.
5. Clear the whole table at the guard-reset point inside `RetireWin32AotGuestPage`.
6. Connect hit, insert, and invalidation counters to the execution summary.
7. Add a synthetic probe covering the hash and mask, insertion and overwrite, the empty-slot
   convention, invalidation, byte-for-byte equality with the probe disabled, fixup re-offsetting,
   and **executing the emitted sequence** to confirm a hit leaves EIP and ESP exactly as the
   original RET would.
8. Build `repiu_aot_probe` and `repiu` for Win32 x86 Debug and Release and run the full pumpit8
   probe.

## Completion criteria

With the feature off, cache bytes and existing logs are unchanged. With it on, return fallbacks
and index scans stay at zero, and the probe confirms the hit path reproduces the original RET's
EIP and ESP effect. Performance judgement is not part of this task: it needs the same section
reproduced three times and belongs to a user run.
