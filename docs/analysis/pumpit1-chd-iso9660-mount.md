# pumpit1 CHD/ISO9660 mount 분석 / pumpit1 CHD/ISO9660 Mount Analysis

## 확인됨 / Confirmed

`roms/pumpit1/19990930.chd`는 CHD v5 CD-ROM image입니다. header는 logical unit `0x990`(2,448 bytes), hunk `0x4C80`(19,584 bytes), 즉 hunk당 raw CD frame 8개를 나타냅니다. metadata tag는 `CHT2`이고 Mode2 raw sector의 user data offset 24에서 ISO9660 primary volume descriptor `CD001`을 확인했습니다.

```mermaid
flowchart LR
    H[CHD v5 hunk 19584] --> F[8 x raw frame 2448]
    F --> M[Mode2 raw data offset 24]
    M --> I[ISO9660 sector 2048]
    I --> R[root directory]
    R --> P[PIU/PIU.EXE]
```

ISO9660 tree는 120개 파일, marker 포함 13,667,761 bytes의 mount cache로 materialize됐습니다. mounted `PIU.EXE` SHA-256은 `5F78A3CCB820327111DC961825E5FAFF5926B0C5483BC77C0A61110A61B797CF`이며 기존 분석 asset과 동일합니다. `DOS4GW.EXE`도 byte-identical입니다.

`pumpit1.zip`에는 `mk3_1.0_bios.u22`, `mk3_1.1_bios.u22`, `piu10.u8`, `piu10.u9`가 존재합니다. 10초 supervisor 실행은 CHD mount cache를 VFS root로 사용해 정상 진행하고 supervisor exit 124로 종료됐습니다.

## English

The pumpit1 asset is a CHD v5 Mode2 raw CD image: 2,448-byte units and 19,584-byte hunks, or eight frames per hunk. ISO9660 user data begins at raw-sector offset 24. Materialization produced 120 files, and mounted `PIU.EXE` and `DOS4GW.EXE` are byte-identical to the previously analyzed copies. A ten-second supervised run uses the CHD-derived root and progresses normally.
