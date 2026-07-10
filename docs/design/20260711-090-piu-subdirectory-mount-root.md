# PIU 하위 디렉터리 실행과 마운트 루트 분리

`piu_1st` 원본 실행 파일은 `MASTER\PIU_1ST\PIU\PIU.EXE`에 위치한다. DOS 가상 파일 시스템의 드라이브 루트는 `MASTER\PIU_1ST`로 유지하고, 초기 current directory만 `\PIU`로 설정한다. 이렇게 하면 guest 관점의 프로그램 위치는 `C:\PIU\PIU.EXE`가 된다.

기존 `TargetProfile::working_directory`는 DOS 가상 드라이브 루트처럼 사용되고 있었다. 이제 `asset_root`를 VFS 마운트 루트로 사용하고, `working_directory`는 초기 실행 current directory로 해석한다. direct executable profile은 기존 동작을 유지하기 위해 `asset_root == working_directory`로 둔다.

DOS absolute path는 기존 DOS 규칙을 따른다. 예를 들어 guest가 `\datas\bga`로 `chdir`하면 현재 디렉터리 `C:\PIU`가 아니라 드라이브 루트 `C:\` 기준으로 해석되어 host `MASTER\PIU_1ST\DATAS\BGA`를 찾는다. 이 동작은 원본 실행 파일의 경로 요청을 숨기지 않기 위해 그대로 유지한다.

검증은 `piu_1st` target 실행 로그에서 VFS root가 `MASTER\PIU_1ST`, VFS current directory가 `\PIU`, executable path가 `MASTER\PIU_1ST\PIU\PIU.EXE`로 출력되는지 확인한다. 이후 DOS path trace로 실제 guest path 요청이 어떤 root 기준으로 해석되는지도 확인한다.

# PIU subdirectory execution and mount root separation

The `piu_1st` original executable is located at `MASTER\PIU_1ST\PIU\PIU.EXE`. The DOS virtual filesystem drive root remains `MASTER\PIU_1ST`, while only the initial current directory is set to `\PIU`. This makes the program location appear to the guest as `C:\PIU\PIU.EXE`.

`TargetProfile::working_directory` was previously used like the DOS virtual drive root. It is now interpreted as the initial execution current directory, while `asset_root` is used as the VFS mount root. The direct executable profile keeps the previous behavior by setting `asset_root == working_directory`.

DOS absolute paths keep DOS semantics. For example, if the guest calls `chdir` with `\datas\bga`, the path is resolved from the drive root `C:\`, not from the current directory `C:\PIU`, and the host path becomes `MASTER\PIU_1ST\DATAS\BGA`. This behavior is preserved so the original executable's path requests remain visible.

Verification checks that the `piu_1st` target log reports VFS root `MASTER\PIU_1ST`, VFS current directory `\PIU`, and executable path `MASTER\PIU_1ST\PIU\PIU.EXE`. The DOS path trace then confirms how actual guest path requests are resolved against the root.
