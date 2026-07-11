# 점진적 Glide signature catalog 작업 지시

1. 플랫폼 공용 Glide signature/return-kind catalog를 추가합니다.
2. asset `@N`과 catalog stack byte count를 gate 호출 시 교차 검증합니다.
3. Win32 x87 `ST(0)` float push helper를 별도 파일로 구현합니다.
4. screen width/height 반환을 논리 surface 크기와 연결합니다.
5. 빌드와 GUI 실행으로 다음 미구현 export까지 관찰합니다.
6. 분석, 아키텍처와 작업 로그를 갱신하고 커밋합니다.

# Incremental Glide Signature Catalog Work Order

Add a platform-neutral signature/return-kind catalog; cross-check catalog stack bytes against asset `@N`; implement a separate Win32 x87 ST(0) float-push helper; connect screen width/height to the logical surface; build and run through the next unimplemented export; update analysis, architecture, and work logs; and commit.
