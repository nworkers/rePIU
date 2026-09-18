# Task 713 작업 지시 — `mov r8h,[esp+d]` long-mode lowering의 DL 저장 방향

설계: [20260918-713](../design/20260918-713-high-byte-destination-lowering.md)

## 범위

Linux x64에서 텍스처 업로드가 빠지는 원인을 찾고 고친다. 추적에 필요한 관측(probe의
도달 기록)을 더하고, 찾은 결함 한 가지를 고친다.

## 단계

1. `REPIU_EXECUTION_PROBE_LOG_ARRIVALS=N` — execution probe가 첫 도달만이 아니라 첫
   N번 도달을 기록하게 한다(레지스터, 메모리 창 16바이트).
2. 두 host에서 같은 지점을 probe해 갈라지는 값을 찾는다.
3. `kStackPointerHighByteDestinationToR15` lowering의 첫 명령을 `41 88 D6`으로
   고친다.
4. probe: `long_mode_compatibility`의 기대 바이트와 해독 의미 검사,
   `long_mode_lowering`의 기대 바이트와 실행 검사.
5. kb(`x86-32bit-encodings-in-long-mode.md`), EXE_DESIGN, frontier, 작업 로그.

## 검증

* 두 host core probe 전체, 새 검사의 음성 확인
* Linux 30초: Glide gate 열, 삼각형 텍스처·픽셀, 폴트
* Win32 30초: Task 712 결과와 비교

## 완료 조건

* Linux의 Glide gate #51이 `_GRTEXTEXTUREMEMREQUIRED@8`이다.
* Win32 회귀가 없다.

---

## English

Design: [20260918-713](../design/20260918-713-high-byte-destination-lowering.md)

### Scope

Find and fix why Linux x64 skips the texture upload: add the observation the trace
needs (arrival logging in the execution probe) and fix the one defect found.

### Steps

1. `REPIU_EXECUTION_PROBE_LOG_ARRIVALS=N`: the execution probe logs the first N
   arrivals (registers and 16 bytes of each memory window), not only the first.
2. Probe the same points on both hosts to find the diverging value.
3. Change the first instruction of the `kStackPointerHighByteDestinationToR15`
   lowering to `41 88 D6`.
4. Probes: the expected bytes and a decode-based semantic check in
   `long_mode_compatibility`; the expected bytes and an execution check in
   `long_mode_lowering`.
5. The kb (`x86-32bit-encodings-in-long-mode.md`), EXE_DESIGN, frontier and work
   log.

### Verification

* Every core-probe group on both hosts, with the new checks shown failing before the
  fix.
* Linux 30 s: Glide gates, triangle texture state and pixels, faults.
* Win32 30 s: compared against Task 712.

### Done when

* Linux's Glide gate #51 is `_GRTEXTEXTUREMEMREQUIRED@8`.
* Win32 shows no regression.
