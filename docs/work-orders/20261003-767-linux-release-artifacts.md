# 작업 지시 767: GitHub 릴리스에 Linux i386·x64 포함

설계: [20261003-767-linux-release-artifacts.md](../design/20261003-767-linux-release-artifacts.md)

## 작업 항목

1. `CMakeLists.txt`: `REPIU_STATIC_RUNTIME` 옵션.
2. `scripts/build_linux_i386.sh`, `scripts/build_linux_x64.sh`: `--static-runtime` 인자.
3. `scripts/package_release_linux.sh` 신규.
4. `.github/workflows/release.yml`: `win32` / `linux`(i386, x64) / `publish` job으로 분리.
5. `docs/guides/release-and-ci.md`: Linux 아티팩트, 로컬 재현, 실행에 필요한 패키지.
6. 작업 로그.

## 완료 조건

* 로컬 x64 Release(`--static-runtime`)의 `repiu`가 `libstdc++`를 동적으로 링크하지 않고,
  패키지 tar.gz가 만들어진다.
* 브랜치에서 수동 실행한 `release.yml`의 세 job이 모두 성공하고 Linux 아티팩트 두 개가 올라온다.

---

# Work Order 767: Linux i386 and x64 in the GitHub release

Design: [20261003-767-linux-release-artifacts.md](../design/20261003-767-linux-release-artifacts.md)

## Tasks

1. `CMakeLists.txt`: the `REPIU_STATIC_RUNTIME` option.
2. `scripts/build_linux_i386.sh`, `scripts/build_linux_x64.sh`: a `--static-runtime` argument.
3. New `scripts/package_release_linux.sh`.
4. `.github/workflows/release.yml`: split into `win32` / `linux` (i386, x64) / `publish` jobs.
5. `docs/guides/release-and-ci.md`: the Linux artifacts, reproducing them locally, and the
   packages needed to run them.
6. The work log.

## Done when

* The local x64 Release `repiu` built with `--static-runtime` does not link `libstdc++`
  dynamically, and the package tar.gz is produced.
* A manual `release.yml` run on the branch finishes all three jobs green and uploads the two
  Linux artifacts.
