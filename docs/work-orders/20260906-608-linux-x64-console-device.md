# 작업 지시 20260906-608 — Linux x64 DOS `CON` device HLE

## 목표

사용자 실행 로그에서 확인된 `INT 21h AH=3Dh`의 `con` open 실패를 DOS
character device 의미에 맞는 공용 HLE로 수정하고, Linux x64 `pumpit2a`가
초기화 오류 경로를 넘어가는지 검증합니다.

## 작업 순서

1. `DosOpenFileHandle`에 device 종류를 표현하는 최소 상태를 추가합니다.
2. `OpenDosFile`에서 관찰된 `CON` 이름을 regular-file existence 검사보다
   먼저 처리하고, 일반 DOS user handle을 할당합니다.
3. traced `AH=40h`가 `CON` handle을 console output으로 전달하도록 연결합니다.
4. `dos_file_create_probe` 또는 독립 probe에 open/no-host-file 및 write-output
   검사를 추가합니다.
5. Linux x64 build, core probe, 실제 `pumpit2a`를 실행하고 path trace와
   종료 상태를 기록합니다.
6. `CON` open 이후에도 초기화 오류가 남으면 `REPIU_DOS_INT_TRACE=1`의
   게스트 EIP와 호출·반환 레지스터를 사용해 다음 분기를 귀속합니다.
7. 아키텍처·analysis·work-log 문서를 갱신하고 task branch에 커밋합니다.

## 안전성 조건

* host에 `CON` 파일을 생성하지 않습니다.
* 특정 guest EIP를 하드코딩하지 않습니다.
* `CON` read와 keyboard input은 이번 근거만으로 구현하지 않습니다.
* 기존 regular-file HLE와 i386 실행 경로를 보존합니다.

## 완료 조건

* `con` open이 DOS error `0x0002`가 아닌 사용자 handle을 반환합니다.
* `CON` write가 console output으로 전달되고 regular-file write 통계에
  섞이지 않습니다.
* 기존 probe와 Linux x64 core probe가 통과합니다.
* 실제 실행에서 초기화 오류 종료의 변화와 다음 frontier가 작업 로그에
  명확히 기록됩니다.

## English

### Objective

Replace the observed `INT 21h AH=3Dh` failure for `con` with a shared HLE that
preserves DOS character-device semantics, then verify whether Linux x64
`pumpit2a` advances beyond its initialization error path.

### Sequence

1. Add the minimum device-kind state to `DosOpenFileHandle`.
2. Handle the observed `CON` name in `OpenDosFile` before regular-file
   existence checks and allocate a normal DOS user handle.
3. Route a `CON` handle from traced `AH=40h` to console output.
4. Add open/no-host-file and write-output coverage to the file probe or a small
   independent probe.
5. Run the Linux x64 build, core probe, and real `pumpit2a`, recording path trace
   and termination state.
6. If the initialization error remains after `CON` opens, attribute the next
   branch using guest EIP and call/return registers from
   `REPIU_DOS_INT_TRACE=1`.
7. Update architecture, analysis, and work-log documents and commit the branch.

### Safety constraints

* Do not create a host file named `CON`.
* Do not hard-code a guest EIP exception.
* Do not implement `CON` reads or keyboard input without evidence in this task.
* Preserve regular-file HLE and the i386 execution path.

### Done criteria

* Opening `con` returns a user handle rather than DOS error `0x0002`.
* Writing through that handle reaches console output and is not counted as a
  regular-file write.
* Existing probes and the Linux x64 core probe pass.
* The work log records the real-run change and, if still present, the separate
guest-initialization frontier clearly.

## 진단 경계

Task 608의 결과는 guest 전체 초기화가 완료되었다는 의미가 아닙니다.
`CON`이 `0x0005` handle로 열리고 console sink에 쓰기가 전달되며, 첫 번째
독립 blocker가 x64에서 호환되지 않는 선택적 direct Glide patch라는 점을
확정했습니다. capability fallback과 이후 DPMI frontier는 후속 작업 지시서로
분리합니다.

## English

## Diagnostic boundary

The Task 608 result is not considered a full guest-initialization fix. It
establishes that `CON` opens as handle `0x0005`, that the write reaches the
console sink, and that the first independent blocker is the x64-incompatible
optional direct Glide patch. The capability fallback and the subsequent DPMI
frontier require a follow-up work order.
