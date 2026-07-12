# Glide state blob 교차 검증 작업 지시

1. PIU의 `grGlideGetState` 호출부와 state buffer allocation을 역추적합니다.
2. `grGlideSetState` 호출부와 blob 직접 접근 여부를 찾습니다.
3. 공개 Glide 문서와 독립 wrapper의 312-byte 후보를 PIU 증거와 교차 검증합니다.
4. DOS/32A에는 관련 Glide state 근거가 없음을 문서화합니다.
5. 검증된 크기와 소비 방식에 맞는 플랫폼 중립 state image 설계를 확정합니다.
6. 안전한 범위만 구현하고 Win32 x86 빌드·실행으로 다음 frontier를 확인합니다.
7. 분석·아키텍처·작업 로그를 갱신하고 커밋합니다.

# Glide State Blob Cross-Validation Work Order

Trace PIU's `grGlideGetState` caller and buffer allocation; locate `grGlideSetState` and any direct blob accesses; cross-check the public 312-byte Glide2 candidate against PIU evidence; record that DOS/32A contains no relevant Glide-state evidence; finalize a platform-neutral state-image design; implement only the validated scope; build and run Win32 x86 to the next frontier; update analysis, architecture, and the work log; and commit.
