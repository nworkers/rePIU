# 20260810-466 pumpito MP3 기본 지연 제거 설계 / Remove pumpito Default MP3 Latency Design

## 한국어

`pumpito` target profile의 50 ms MP3 시작 지연을 제거하고 모든 PIU10 profile의 기본값을
0 ms로 통일합니다. 가변 지연 기능은 유지하며 `REPIU_PIU10_MP3_LATENCY_MS`를 명시한 실행에만
0~500 ms 범위로 적용합니다. profile regression probe는 새 0 ms 기본값을 검증하고, 50 ms를
8,820 byte 무음으로 변환하는 독립 검증은 override 기능을 위해 유지합니다.

## English

Remove the 50 ms MP3 startup latency from the `pumpito` target profile so every PIU10 profile
defaults to 0 ms. Keep variable latency support and apply a 0–500 ms delay only when
`REPIU_PIU10_MP3_LATENCY_MS` is explicitly set. Update the profile regression probe for the new
0 ms default while retaining the independent 50 ms to 8,820-byte conversion check for override
coverage.
