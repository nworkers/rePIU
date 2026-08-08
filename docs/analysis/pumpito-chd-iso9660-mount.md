# pumpito CHD/ISO9660 마운트 분석 / pumpito CHD/ISO9660 Mount Analysis

## 한국어

### 확인됨

- 현재 로컬 `roms/pumpito/OBGSE.chd`는 CHT2 `TYPE:MODE1`, 68,568 frames인 단일 data
  track입니다. logical data start는 LBA 0이고 lead-out은 68,568입니다.
- CHD v5 logical size는 167,854,464 bytes, unit size는 2,448 bytes입니다. CHD SHA-256은
  `26DC1B7842A44CFCEA895F86DB4F9E3115487BF5C13132F755BFB8D4A8D3BD81`이고,
  `chdman verify`의 raw SHA-1과 overall SHA-1 검증이 모두 성공했습니다.
- CHD unit은 2,448 bytes이지만 ISO9660 user data는 cooked Mode 1 layout인 offset 0에서
  시작합니다. 상대 LBA 16에 `01 CD001 01` PVD가 있습니다.
- root self-record로 확인한 extent-LBA bias는 `0`입니다.
- `chdman extractcd`는 `TRACK 01 MODE1/2048`로 140,427,264-byte 이미지를 복원했습니다.
  복원 이미지와 `OBGSE.iso`의 SHA-256은 모두
  `29C3BD41804252C92919852289F0992700C3B410D562CF7678F70D0BC19EC3E3`으로 완전히 같습니다.
- 현재 ISO의 실행 경로는 `PIU/PIU.EXE`이며 작업 디렉터리는 `PIU` 디렉터리입니다.
- mount cache에는 860개 파일, 278,680,160 bytes가 추출됐습니다. 선택된 `PIU/PIU.EXE`는
  1,696,368 bytes이며 SHA-256은
  `7E396E4652F51FD4292C1707FD328DF67C2BFB9AF559C531B8393FCC9CE3CF5D`입니다.
- DOS/4GW analyzer는 4개 LE object, runtime entry `0x000FEADC`, stack top `0x009B1650`,
  relocation failure 0건으로 완료됐습니다.
- Task 451의 PIU10 ISA HLE 후 guest는 이전 `0x0402106D`/port `0x02DA` blocker를 넘었습니다.
  3초 실행은 port I/O 32건을 모두 처리하고 예외 없이 timeout됐으며, 10초 실행은
  `BGA\\00.DAT` open/read까지 진행했습니다.

### 미확정

- `pumpito`의 MP3 출력, 렌더링, 입력 및 전체 플레이 동작은 아직 검증하지 않았습니다.

## English

### Confirmed

- The current local `roms/pumpito/OBGSE.chd` has one data track with CHT2 `TYPE:MODE1` and
  68,568 frames. Its logical data start is LBA 0 and its lead-out is 68,568.
- The CHD v5 logical size is 167,854,464 bytes with 2,448-byte units. Its SHA-256 is
  `26DC1B7842A44CFCEA895F86DB4F9E3115487BF5C13132F755BFB8D4A8D3BD81`; both raw SHA-1 and
  overall SHA-1 verification pass in `chdman verify`.
- Although each CHD unit is 2,448 bytes, ISO9660 user data begins at offset 0 in the cooked Mode 1
  layout. The `01 CD001 01` PVD is present at relative LBA 16.
- Root self-record discovery found an extent-LBA bias of `0`.
- `chdman extractcd` restores a 140,427,264-byte image as `TRACK 01 MODE1/2048`. The restored
  image and `OBGSE.iso` have the identical SHA-256
  `29C3BD41804252C92919852289F0992700C3B410D562CF7678F70D0BC19EC3E3`.
- The current ISO executable path is `PIU/PIU.EXE`, and its working directory is `PIU`.
- The mount cache contains 860 extracted files totaling 278,680,160 bytes. The selected
  `PIU/PIU.EXE` is
  1,696,368 bytes with SHA-256
  `7E396E4652F51FD4292C1707FD328DF67C2BFB9AF559C531B8393FCC9CE3CF5D`.
- The DOS/4GW analyzer completed with four LE objects, runtime entry `0x000FEADC`, stack top
  `0x009B1650`, and zero relocation failures.
- With Task 451's PIU10 ISA HLE, the guest passes the former `0x0402106D`/port `0x02DA` blocker.
  A three-second run handled all 32 port accesses and timed out without an exception; a ten-second
  run progressed through opening and reading `BGA\\00.DAT`.

### Unresolved

- `pumpito` MP3 output, rendering, input, and full gameplay remain unverified.
