# 20260802-394 pumpit2 ZIP+CHD 프로필 작업 지시 / pumpit2 ZIP+CHD Profile Work Order

설계: [20260802-394-pumpit2-zip-chd-profile.md](../design/20260802-394-pumpit2-zip-chd-profile.md)

## 한국어

1. `TargetProfile`에 공용 `rom_set_id`를 추가하고 `pumpit1`, `pumpit2` profile을 구성합니다.
2. `pumpit1_mount`를 공용 PIU CHD mount subsystem으로 이름과 API를 일반화합니다.
3. ISO reader가 CHD track table의 data track을 기준으로 상대 LBA를 변환하도록 수정합니다.
4. `ChdCdImage`에 안정적인 CHD identity 조회를 추가합니다.
5. PIU10/YMZ280B sample ROM API의 `pumpit1` 전용 이름을 공용 이름으로 바꿉니다.
6. loader와 analyzer를 `rom_set_id` 기반 mount로 연결합니다.
7. pumpit1/pumpit2 analyzer, Release build/probe, 양쪽 loader smoke를 검증합니다.
8. 분석 문서, 색인, 아키텍처와 작업 로그를 갱신하고 커밋합니다.

## English

1. Add shared `rom_set_id` metadata to `TargetProfile` and configure pumpit1/pumpit2 profiles.
2. Generalize the pumpit1 mount name and API into a shared PIU CHD mount subsystem.
3. Resolve ISO-relative LBAs from the CHD track table's data track.
4. Expose stable CHD identity from `ChdCdImage`.
5. Rename pumpit1-specific PIU10/YMZ280B sample-ROM APIs to shared terminology.
6. Wire loader and analyzer through `rom_set_id`.
7. Verify pumpit1/pumpit2 analyzers, Release build/probe, and both loader smokes.
8. Update analysis, index, architecture, and work log, then commit.
