# 작업 로그: pumpit2 ZIP/CHD profile

## 결과

- `TargetProfile::rom_set_id`로 ROM-set mount 여부를 선언하고 `pumpit1`과
  `pumpit2`를 동일한 orchestration 경로에 연결했습니다.
- `pumpit1_mount`를 `piu_chd_mount`로 일반화하고 loader/analyzer의 게임 ID
  비교를 제거했습니다.
- `ChdCdImage`의 공용 track table과 identity를 mount에서도 재사용했습니다.
- 데이터 트랙 PVD와 ISO extent 주소 체계를 분리했습니다. 멀티세션 이미지에서는
  루트 자기 참조를 스캔해 extent bias를 구하며, 데이터 트랙 밖 파일 extent는
  추출하지 않습니다.
- PIU10 sample-ROM API의 `pumpit1` 전용 이름을 공용 이름으로 변경했습니다.

## 검증

- Release build: `repiu_exe_analyzer`, `repiu_loader_win32`,
  `repiu_aot_probe`, `repiu_chd_cd_probe` 성공
- `repiu_exe_analyzer pumpit1`: 성공, 기존 cache 재사용
- `repiu_exe_analyzer pumpit2`: 성공, LE relocation failure 0
- pumpit2 최초 mount: 332 files / 35,408,346 bytes / 62 external extents skipped
- pumpit2 재실행: CHD identity 및 `-11400` bias cache 재사용 성공
- pumpit1 `aot-dbt` 3초 smoke: 기존 실행 경로 진행 및 timeout 종료
- pumpit2 `legacy` 3초 smoke: `INTRO.ANI`, `STAGE.CFG` 읽기, YMZ280B 및
  63-track MSCDEX 초기화, timeout 종료
- pumpit2 AOT probe: translation plan 생성 성공, 정적 cache 밖 direct target
  한 건 때문에 code-cache executable 생성은 실패

## 회고와 후속

`pumpit2` 차이는 게임별 실행 주소가 아니라 멀티세션 CD의 저장 주소 체계였습니다.
루트 레코드 기반 bias 탐색으로 이를 공용 mount 계층에 흡수했습니다. 후속 작업은
pumpit2 AOT direct-target 한 건을 분석하고, 장시간 실제 플레이에서 화면·입력·CD-DA를
검증하는 것입니다.

# Work log: pumpit2 ZIP/CHD profile

## Result

- Added declarative ROM-set metadata to target profiles and routed pumpit1 and
  pumpit2 through one shared CHD mount path.
- Generalized the mount and PIU10 sample-ROM APIs, reused `ChdCdImage`, and
  removed game-ID branching from loader/analyzer orchestration.
- Added multisession ISO extent translation by root self-record discovery and
  skipped external audio extents outside the data track.

## Verification

- All four affected Release targets built successfully.
- Both analyzers passed; pumpit2 had zero relocation failures.
- Initial and cached pumpit2 mounts passed with the counts recorded above.
- Pumpit1 AOT-DBT regression smoke and pumpit2 legacy smoke reached guest
  execution and ended at their configured timeout.
- Pumpit2 AOT planning exposed one unresolved static-cache external direct
  target, recorded for follow-up rather than weakened with a title-specific
  exception.
