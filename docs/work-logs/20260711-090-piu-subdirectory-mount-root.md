# PIU 하위 디렉터리 실행과 마운트 루트 분리 작업 로그

## 결과

`piu_1st` target의 원본 실행 파일 위치를 `MASTER\PIU_1ST\PIU\PIU.EXE`로 갱신했다. DOS VFS는 `MASTER\PIU_1ST`를 드라이브 루트로 마운트하고, 초기 current directory는 `\PIU`가 되도록 `asset_root`와 `working_directory` 해석을 분리했다.

`DosVirtualFileSystemState` 초기화는 host root와 initial current directory를 별도로 받는 overload를 추가했다. Initial current directory가 host root 밖으로 나가면 VFS 초기화를 실패 상태로 남긴다.

Win32 loader 로그에는 DOS VFS current directory를 추가로 출력한다. 따라서 `piu_1st` 실행 시 guest 관점에서 `C:\PIU\PIU.EXE` 형태의 배치가 되었는지 바로 확인할 수 있다.

## 관찰

새 배치에서 `piu_1st`의 초기 DOS path 상태는 다음과 같다.

1. executable: `MASTER/PIU_1ST/PIU/PIU.EXE`
2. VFS root: `MASTER\PIU_1ST`
3. VFS current directory: `\PIU`

실행 중 원본은 먼저 `chdir \datas\bga`를 요청한다. DOS absolute path 규칙에 따라 이 경로는 `C:\PIU\datas\bga`가 아니라 `C:\datas\bga`로 해석되며, host에서는 `MASTER\PIU_1ST\DATAS\BGA`를 찾는다. 현재 실제 파일 배치에는 이 디렉터리가 없으므로 `0x0003` 실패가 기록된다.

그 다음 원본은 상대 경로 `intro.ani`를 열고, 이 요청은 현재 디렉터리 `\PIU` 기준으로 `MASTER\PIU_1ST\PIU\INTRO.ANI`에 매핑되어 성공한다. 따라서 기존 파일 open 실패 지점은 넘어갔고, 현재 다음 blocker는 open handle `0x0005`를 대상으로 한 `INT 21h AH=3Fh` file read HLE 미구현 지점이다.

## 검증

다음 명령으로 검증했다.

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

결과는 통과했다. Win32 x86 빌드 중 third-party `spdlog` header에서 기존 코드 페이지 경고 `C4819`가 계속 출력되지만, 빌드 실패는 아니다.

# PIU subdirectory execution and mount root separation work log

## Result

Updated the `piu_1st` target executable location to `MASTER\PIU_1ST\PIU\PIU.EXE`. DOS VFS now mounts `MASTER\PIU_1ST` as the drive root and sets the initial current directory to `\PIU` by separating the meanings of `asset_root` and `working_directory`.

Added a `DosVirtualFileSystemState` initialization overload that accepts host root and initial current directory separately. If the initial current directory escapes the host root, VFS initialization is left in a failed state.

The Win32 loader now logs the DOS VFS current directory. This makes it easy to confirm that `piu_1st` is laid out as `C:\PIU\PIU.EXE` from the guest perspective.

## Observation

The new `piu_1st` initial DOS path state is:

1. executable: `MASTER/PIU_1ST/PIU/PIU.EXE`
2. VFS root: `MASTER\PIU_1ST`
3. VFS current directory: `\PIU`

During execution, the original program first requests `chdir \datas\bga`. Under DOS absolute path rules this resolves to `C:\datas\bga`, not `C:\PIU\datas\bga`, so the host path is `MASTER\PIU_1ST\DATAS\BGA`. That directory is not present in the current physical layout, so the request records failure `0x0003`.

The original program then opens relative path `intro.ani`. This resolves from current directory `\PIU` to `MASTER\PIU_1ST\PIU\INTRO.ANI` and succeeds. Therefore the previous file-open failure point has moved forward; the current next blocker is the unimplemented `INT 21h AH=3Fh` file read HLE for open handle `0x0005`.

## Verification

Verified with:

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

The verification passed. The Win32 x86 build still reports the existing third-party `spdlog` code-page warning `C4819`, but it is not a build failure.
