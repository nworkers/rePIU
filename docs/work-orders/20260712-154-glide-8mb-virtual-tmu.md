# Glide 8 MiB virtual TMU 작업 지시

1. 플랫폼 공용 Glide hardware profile에 8 MiB TMU 용량을 추가합니다.
2. 마지막 8-byte aligned texture start 계산을 공용 helper로 구현합니다.
3. `grTexMaxAddress(GR_TMU0)` typed signature와 EAX 반환을 연결합니다.
4. 빌드와 GUI 실행으로 다음 실제 texture API까지 관찰합니다.
5. 분석과 작업 로그를 갱신하고 커밋합니다.

# Glide 8 MiB Virtual TMU Work Order

Add an 8 MiB TMU capacity to the shared Glide hardware profile, calculate the final 8-byte-aligned texture start in shared code, connect the typed `grTexMaxAddress(GR_TMU0)` EAX return, build and run through the next real texture API, update analysis/work logs, and commit.
