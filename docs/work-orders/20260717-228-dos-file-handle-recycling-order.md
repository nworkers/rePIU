# 작업 지시: DOS 파일 핸들 번호 재사용
# Work Order: DOS File Handle Number Recycling

관련 설계: `docs/design/20260717-228-dos-file-handle-recycling.md`
관련 frontier: `docs/analysis/current-execution-frontier.md` Task 227 절

## 1. 목표 / Goal

DOS INT 21h 파일 open이 **가장 낮은 free 핸들**을 반환하고 close 시 그 번호를 재사용
하도록 HLE를 고쳐, 게스트 clib의 20칸 핸들 테이블을 오버플로우하는 근인
(`0x030FAB04` fault, VA `0x00004091`)을 제거한다.

## 2. 변경 대상 / Files

* `include/repiu/hle/dos_file_system.h`
  - `DosVirtualFileSystemState::next_file_handle` 필드 제거.
* `src/hle/dos_file_system.cpp`
  - 파일-정적 상수 `kFirstDosUserHandle=5`, `kDosOpenHandleLimit=20` 추가.
  - lowest-free 핸들 탐색 헬퍼 추가.
  - `OpenDosFile`: 단조 증가 대신 lowest-free 할당 + 닫힌 슬롯 재사용.
* `tests/` (해당 위치)
  - DOS 파일 핸들 할당/회수 단위 테스트 추가.

## 3. 구현 단계 / Steps

1. `next_file_handle` 필드 제거 및 참조 정리.
2. lowest-free 할당 로직 구현(범위 `[5, 20)`, 없으면 exhausted 실패).
3. 닫힌 `open_files` 슬롯 재사용으로 벡터 증가 억제.
4. 단위 테스트 작성(순차 open/close가 핸들을 낮게 유지, 동시 다수 open, 상한 초과 실패,
   close 후 재사용).
5. 빌드(`build/win32_x86_dpmi`).
6. `aot-dynamic pumpit1` 구동으로 크래시 소멸·전진 확인.
7. trap 백엔드 단시간 회귀 확인.

## 4. 완료 기준 / Done criteria

* fault VA `0x00004091` / guest `0x030FAB04` 크래시 소멸.
* `last DOS open handle`가 20 미만 유지.
* `dispatch_entry`가 이전 크래시 지점을 넘어 전진(회귀 없음).
* 단위 테스트 통과, 빌드 성공.
* `docs/analysis/current-execution-frontier.md`, 작업 로그, 메모리 갱신.

---

**English summary.** Make DOS file open return the lowest free handle and recycle numbers on
close, removing the guest 20-entry handle-table overflow (Task 227 frontier at `0x030FAB04`,
fault VA `0x00004091`). Edit `dos_file_system.{h,cpp}` (drop `next_file_handle`, add
lowest-free allocation in `[5,20)` with closed-slot reuse), add unit tests, build under
`build/win32_x86_dpmi`, and verify the crash is gone and execution advances via an
aot-dynamic `pumpit1` run plus a trap-backend regression check.
