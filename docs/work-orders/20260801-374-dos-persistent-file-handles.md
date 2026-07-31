# 작업 지시: DOS 파일 핸들 지속화 / Work order: persistent DOS file handles

Task 374. 설계: [20260801-374](../design/20260801-374-dos-persistent-file-handles.md)

## 한국어

### 목표

DOS 읽기가 호스트 파일을 매번 다시 열지 않게 한다. 핸들이 살아 있는 동안 스트림을
유지하고, 파일 크기는 열 때 1회만 조회한다.

### 단계

1. **`DosHostFileCache` 추가** (`dos_file_system.{h,cpp}`)
   * `std::unique_ptr<std::ifstream>` 보유.
   * **복사 시 차갑게 시작**(스트림을 공유하지 않음), 이동은 기본.
     상태가 복사 대입되므로 복사 가능성은 필수.
   * `Acquire(path, offset)` — 없으면 열고, 요청 위치로 seek한 스트림 반환.
     실패 시 nullptr.
   * `Reset()`, `warm()`, `reopen_count()`.

2. **`DosOpenFileHandle` 확장**
   * `DosHostFileCache host_stream`
   * `std::uint64_t cached_file_size` — `OpenDosFile`에서 1회 설정.

3. **`ReadDosFile` 수정**
   * `std::filesystem::file_size` 호출 제거, `cached_file_size` 사용.
   * `host_stream.Acquire(...)`로 스트림 확보 후 read.
   * EOF·짧은 읽기·오류 반환값은 **기존과 동일**해야 한다.

4. **`SeekDosFile` 수정**
   * SEEK_END에서 `cached_file_size` 사용, stat 제거.

5. **`CloseDosFile` 수정**
   * 캐시 폐기.

6. **관측 추가**
   * `DosVirtualFileSystemState`에 `file_read_count`, `host_file_open_count`.
   * 요약 한 줄: `Win32 DOS file reads/host opens/reopen ratio`.

7. **probe 추가**
   * `dos_file_handle_cache_probe.{h,cpp}` + `main.cpp` + `CMakeLists.txt`.
   * 검증: 연속 읽기가 호스트 열기 1회만 유발, 복사본은 차갑게 시작하고 원본과
     위치를 공유하지 않음, close 후 캐시 폐기, 열기 실패 시 기존 오류 코드,
     EOF·짧은 읽기 동작 불변.

8. **빌드·측정**
   * Debug + Release, probe exit 0.
   * 음악 선택 화면을 `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1`,
     `REPIU_GLIDE_SWAP_INTERVAL=0`으로 재캡처.
   * 판정 지표: `0x030F87B7`의 호출당 cycle, DOS wall 비중,
     host opens / reads 비.

9. **문서**
   * 작업 로그, `docs/analysis/dos-file-io-and-int3.md` 갱신.

### 완료 조건

* 같은 핸들의 연속 읽기가 호스트 열기를 **1회만** 유발.
* `0x030F87B7` 호출당 cycle이 25.7M에서 크게 하락.
* 읽기 반환값 의미(EOF, 짧은 읽기, 오류 코드) **불변**.
* probe 통과, 양 구성 빌드 성공.

### 비범위

* 쓰기 API는 존재하지 않으므로 다루지 않는다.
* 게스트 계산량(+29%)과 예외 기구 축은 별건(Task 373).

---

## English

Stop the DOS read path from reopening the host file on every call. Add a
`DosHostFileCache` holding a `std::unique_ptr<std::ifstream>` whose copy semantics
start cold rather than sharing a stream — copyability is mandatory because the
state is copy-assigned into the guest thread context, and sharing would make two
handles share a file position. Give `DosOpenFileHandle` that cache plus a
`cached_file_size` set once at open, then remove the per-read `file_size` stat and
the per-read `ifstream` construction, and use the cached size for SEEK_END. Discard
the cache on close. Add `file_read_count` and `host_file_open_count` with a summary
line so the fix is verifiable, and a probe covering repeated reads causing one open,
copies starting cold without sharing position, cache discard on close, unchanged
error codes on open failure, and unchanged EOF and short-read behaviour. Verify with
both build configurations, a passing probe, and a music-select recapture with the
hotspot profiler and swap interval 0, judged on the per-call cycles at `0x030F87B7`,
the DOS share of wall, and the ratio of host opens to reads.
