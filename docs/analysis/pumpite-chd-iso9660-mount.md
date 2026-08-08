# pumpite CHD/ISO9660 마운트 분석 / pumpite CHD/ISO9660 Mount Analysis

## 한국어

### 확인됨

- 로컬 `roms/pumpite/010209_1821.chd`는 두 개의 `MODE2_RAW` data track을 갖습니다.
- track 1은 logical LBA `0`, 103,106 frames이고 track 2는 logical LBA `103106`,
  210,200 frames입니다.
- 원본 CUE는 두 번째 세션의 INDEX 01을 절대 disc frame `114504`로 기록합니다. CHD logical
  주소와의 차이 `11398`은 CHD에서 생략된 세션 간 영역을 나타냅니다.
- 두 track 모두 각 세션 상대 LBA 16에 ISO9660 `CD001` PVD를 갖습니다.
- 최신 멀티세션 파일 시스템은 마지막 data track에서 선택해야 합니다. 공용 reader는 마지막
  data track을 mount 대상으로 삼고, 기존 PVD 확인과 root extent bias 탐색을 그대로 적용합니다.
- 마지막 track을 선택한 mount는 extent-LBA bias `-11400`을 찾아 736개 파일,
  428,909,991 bytes를 `build/runtime_mounts/pumpite/`에 추출했습니다.
- `PIU/PIU.EXE`는 1,795,551 bytes, SHA-256
  `FBC29B8B0CB8AB5B69F15ED0289109D18B401A1A22CA2C28BF4A0EFF9CEF89CC`입니다.
  DOS/4GW analyzer는 4개 LE object, entry `0x0010E024`, stack top `0x009F0900`,
  relocation failure 0건으로 완료됐습니다.

### 미확정

- `pumpitc`, `pumpitpc`의 로컬 CHD가 없어 각 디스크의 track 수와 실행 파일
  identity는 아직 확인하지 못했습니다.
- `pumpite` guest 기동과 전체 플레이 동작은 아직 검증하지 않았습니다.

## English

### Confirmed

- The local `roms/pumpite/010209_1821.chd` contains two `MODE2_RAW` data tracks.
- Track 1 starts at logical LBA `0` and has 103,106 frames; track 2 starts at logical LBA
  `103106` and has 210,200 frames.
- The source CUE places session two INDEX 01 at absolute disc frame `114504`. Its `11398`-frame
  difference from the CHD logical address represents the inter-session area omitted from the CHD.
- Both tracks contain an ISO9660 `CD001` PVD at relative LBA 16.
- The newest multisession filesystem is selected from the last data track. The shared reader uses
  that track while preserving the existing PVD check and root-extent bias discovery.
- Mounting the last track discovered an extent-LBA bias of `-11400` and extracted 736 files totaling
  428,909,991 bytes under `build/runtime_mounts/pumpite/`.
- `PIU/PIU.EXE` is 1,795,551 bytes with SHA-256
  `FBC29B8B0CB8AB5B69F15ED0289109D18B401A1A22CA2C28BF4A0EFF9CEF89CC`.
  The DOS/4GW analyzer completed with four LE objects, entry `0x0010E024`, stack top
  `0x009F0900`, and zero relocation failures.

### Unresolved

- Local CHDs for `pumpitc` and `pumpitpc` are unavailable, so their track layouts and
  executable identities are not yet confirmed.
- `pumpite` guest startup and full gameplay remain unverified.
