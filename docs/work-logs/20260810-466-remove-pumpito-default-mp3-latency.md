# 20260810-466 pumpito MP3 기본 지연 제거 작업 로그 / Remove pumpito Default MP3 Latency Work Log

설계: [20260810-466-remove-pumpito-default-mp3-latency.md](../design/20260810-466-remove-pumpito-default-mp3-latency.md)

작업 지시: [20260810-466-remove-pumpito-default-mp3-latency.md](../work-orders/20260810-466-remove-pumpito-default-mp3-latency.md)

## 한국어

`pumpito`의 명시적 50 ms profile 지연을 제거하고 regression probe 및 현재 문서를 0 ms
기본값에 맞췄습니다. 환경변수 override와 시간→무음 byte 변환 기능은 유지했습니다.
Win32 x86 Debug `repiu_aot_probe --piu10`은
`piu10_mp3_latency_profile=true,milliseconds=0`과
`piu10_mp3_latency_bytes=true,bytes=8820`을 함께 기록했고 모든 PIU10 항목이 통과했습니다.
Win32 x86 Debug `repiu` 빌드도 성공했습니다. 기존 C4819 경고 외에 새 오류는 없습니다.

## English

Removed the explicit 50 ms profile latency from `pumpito` and aligned the regression probe and
current documentation with the 0 ms default. The environment override and time-to-silence-byte
conversion remain available.
Win32 x86 Debug `repiu_aot_probe --piu10` reports both
`piu10_mp3_latency_profile=true,milliseconds=0` and
`piu10_mp3_latency_bytes=true,bytes=8820`, with every PIU10 check passing. The Win32 x86 Debug
`repiu` build also succeeded with no new errors beyond the existing C4819 warnings.
