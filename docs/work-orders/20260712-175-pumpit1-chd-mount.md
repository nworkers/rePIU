# pumpit1 CHD mount 작업 지시 / Work Order

1. `roms/`를 Git ignore에 추가합니다.
2. BSD 라이선스와 version을 고정한 libchdr reader를 도입합니다.
3. pumpit1 ZIP/CHD discovery와 오류 진단을 구현합니다.
4. CHD logical disk의 partition과 file system을 확인하고 최소 read-only reader를 구현합니다.
5. mount view의 `PIU/PIU.EXE`, `DOS4GW.EXE`, `Glide2x.ovl`을 기존 loader/VFS에 연결합니다.
6. `pumpit1` profile, analyzer 출력, Win32/x86 build와 짧은 실행을 검증합니다.
7. 라이선스·분석·사용법 문서를 갱신합니다.

## English

Ignore ROM assets, integrate a pinned BSD-compatible CHD reader, discover and validate the MAME set, implement the confirmed read-only partition/file-system path, connect the mounted `PIU/PIU.EXE` tree to the existing loader/VFS, and verify build and execution with documentation and license provenance.
