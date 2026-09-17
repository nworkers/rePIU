# Task 697 작업 지시

guest `0x010F777C` / cache `0x200695A3` SIGTRAP의 AOT 경계를 분류합니다.

1. [x] 기존 address-map, fault provenance, reentry compatibility trace를 함께
   실행합니다.
2. [x] guest/cache bytes, code mode, exact/previous provenance와 fixup을
   대조합니다.
3. [x] 정적 cache 주소의 현재/직전 byte, reverse map, provenance를 출력하는 읽기 전용
   진단을 추가합니다. 실행 정책은 바꾸지 않습니다.
4. [x] 분석 및 작업 로그를 갱신하고 조사 결과를 커밋합니다.

## English

Classify the AOT boundary behind the guest `0x010F777C` / cache `0x200695A3`
SIGTRAP.

1. [x] Run the existing address-map, fault-provenance, and re-entry compatibility
   traces together.
2. [x] Correlate guest/cache bytes, code mode, exact/previous provenance, and
   fixups.
3. [x] Add a read-only diagnostic for current/previous bytes, reverse maps, and
   provenance at a static cache address, without changing execution policy.
4. [x] Update the analysis and work log, then commit the investigation result.
