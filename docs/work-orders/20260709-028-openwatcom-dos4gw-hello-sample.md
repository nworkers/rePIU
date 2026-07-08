# OpenWatcom DOS/4GW Hello 샘플 작업 지시

## 작업 항목

1. OpenWatcom 라이선스와 배포 위치를 문서화한다.
2. `tools/openwatcom/`을 로컬 도구 설치 위치로 정하고 Git 추적에서 제외한다.
3. OpenWatcom 설치/확인 스크립트를 유지한다.
4. 샘플 소스를 표준 C `main`/`puts` 기반 `samples/dos4gw_hello/hello.c`로 둔다.
5. `scripts/build_dos4gw_hello.bat`가 `wcl386 -bt=dos -l=dos4g`로 실제 DOS/4G runtime 호환 `hello.exe`를 생성하게 한다.
6. analyzer에서 `hello.exe`의 LE stack object가 유효한지 확인한다.
7. loader가 `dos4gw_hello` target에서 OpenWatcom C runtime startup과 console 출력에 필요한 최소 DOS/DPMI HLE를 처리하게 한다.
8. loader 실행으로 `Hello, world!` 출력과 정상 종료를 검증한다.
9. 기존 `piu_1st` 관찰 지점이 유지되는지 확인한다.
10. 작업 로그를 남긴다.

## 비목표

* OpenWatcom 바이너리 또는 소스 코드 커밋
* 완전한 DOS/DPMI HLE 구현
* 원본 `PIU.EXE` 경로 변경

# OpenWatcom DOS/4GW Hello Sample Work Order

## Tasks

1. Document OpenWatcom licensing and distribution source.
2. Use `tools/openwatcom/` as the local tool install path and exclude it from Git tracking.
3. Keep the OpenWatcom install/check script.
4. Keep the sample source as standard C `main`/`puts` at `samples/dos4gw_hello/hello.c`.
5. Make `scripts/build_dos4gw_hello.bat` generate a real DOS/4G-runtime-compatible `hello.exe` with `wcl386 -bt=dos -l=dos4g`.
6. Confirm with the analyzer that `hello.exe` has a valid LE stack object.
7. Let the loader handle the minimal DOS/DPMI HLE required by OpenWatcom C runtime startup and console output for the `dos4gw_hello` target.
8. Verify `Hello, world!` output and normal termination through loader execution.
9. Confirm that the existing `piu_1st` observation point is preserved.
10. Leave a work log.

## Non-Goals

* Committing OpenWatcom binaries or source code.
* Implementing complete DOS/DPMI HLE.
* Changing the original `PIU.EXE` path.
