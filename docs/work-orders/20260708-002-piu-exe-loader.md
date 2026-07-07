# 작업 지시: PIU.EXE 로더

## 목표

`MASTER\PIU_1ST\PIU.EXE`를 읽고 원본 DOS/4G 보호 모드 코드를 실행할 수 있는 네이티브 Win32 로더를 만든다.

로더는 원본 게임 로직을 보존해야 하며, DOS, DPMI, 파일 시스템, 그래픽, 입력, 타이머, 오디오 서비스는 프로젝트 HLE 계층에서 제공한다.

`PIU_1ST`는 첫 번째 타깃일 뿐이며, 이후 여러 버전을 추가할 수 있도록 타깃 프로파일 구조를 전제로 한다.

## 현재 확인 사항

초기 분석으로 확인한 내용:

* `MASTER\PIU_1ST\PIU.EXE`는 `MZ` 헤더로 시작한다.
* `e_lfanew` 값은 `0x2C90`이다.
* `0x2C90` 위치에는 `LE` 시그니처가 있다.
* 같은 디렉터리에 `DOS4GW.EXE`가 있지만, DOS4GW를 외부 실행기로 사용하는 대신 LE 이미지를 직접 로드하고 필요한 서비스를 HLE로 제공한다.

## 비목표

* 게임플레이 로직을 C++로 재구현하지 않는다.
* DOSBox를 통합하지 않는다.
* 범용 DOS 런타임을 만들지 않는다.
* 문서화된 불가피한 사유 없이 원본 실행 파일 코드를 패치하지 않는다.
* `MASTER\PIU_1ST` 전용 가정을 핵심 로더에 섞지 않는다.

## 구조

초기 로더는 다음 모듈 경계를 가진다.

* `TargetRegistry`: 게임 타깃과 버전 선택
* `TargetProfile`: 실행 파일 경로, 작업 디렉터리, 포맷 힌트, 버전별 메타데이터
* `ExecutableReader`: 원본 파일 읽기
* `MzParser`: DOS MZ 헤더와 LE/LX 위치 파악
* `LeParser`: LE 헤더, 오브젝트 테이블, 페이지 테이블, fixup, 엔트리 포인트 파싱
* `ImageMapper`: 원본 보호 모드 코드가 기대하는 메모리 이미지 구성
* `RuntimeMemory`: 실행 메모리, 스택, 힙, selector 추상화, HLE 영역 관리
* `ExecutionEngine`: 32-bit Win32 프로세스에서 원본 x86 코드로 제어 이전
* `HleDispatcher`: DOS, DPMI, 타이머, 입력, 그래픽, 오디오, 파일 시스템 호출 처리
* `TraceLogger`: 로더 결정, HLE 호출, 예외, 실행 단계 기록

## 마일스톤

1. 비실행 분석 도구를 만든다.
2. LE 이미지를 메모리에 매핑한다.
3. 최소 런타임 셸을 만든다.
4. 제어된 방식으로 엔트리 포인트에 진입한다.
5. 실제 trace에 기반해 최소 DOS/DPMI HLE를 구현한다.
6. 그래픽, 입력, 타이머를 연결한다.
7. 여러 버전을 위한 프로파일 구조를 일반화한다.

## 즉시 다음 작업

먼저 `MASTER\PIU_1ST\PIU.EXE`를 실행하지 않고 분석하는 도구를 구현한다.

이 도구는 MZ/LE 헤더, 오브젝트 테이블, 페이지 테이블, fixup 위치, 엔트리 포인트를 출력해야 한다.

## Work Order: PIU.EXE Loader

## Goal

Build a native Win32 loader that can read `MASTER\PIU_1ST\PIU.EXE` and execute the original DOS/4G protected-mode code.

The loader must preserve the original game logic, while DOS, DPMI, filesystem, graphics, input, timer, and audio services are supplied by the project HLE layers.

`PIU_1ST` is only the first target. The design assumes target profiles so more versions can be added later.

## Current Findings

Initial analysis confirmed:

* `MASTER\PIU_1ST\PIU.EXE` starts with an `MZ` header.
* The `e_lfanew` value is `0x2C90`.
* Offset `0x2C90` contains the `LE` signature.
* `DOS4GW.EXE` exists in the same directory, but the loader should directly load the LE image and provide required services through HLE instead of using DOS4GW as an external runner.

## Non-Goals

* Do not reimplement gameplay logic in C++.
* Do not integrate DOSBox.
* Do not build a generic DOS runtime.
* Do not patch original executable code without a documented unavoidable reason.
* Do not mix `MASTER\PIU_1ST`-specific assumptions into the core loader.

## Structure

The initial loader has these module boundaries:

* `TargetRegistry`: game target and version selection
* `TargetProfile`: executable path, working directory, format hint, and version metadata
* `ExecutableReader`: original file reading
* `MzParser`: DOS MZ header and LE/LX location
* `LeParser`: LE header, object table, page table, fixups, and entry point
* `ImageMapper`: memory image expected by the original protected-mode code
* `RuntimeMemory`: executable memory, stack, heap, selector abstraction, and HLE regions
* `ExecutionEngine`: control transfer to original x86 code in a 32-bit Win32 process
* `HleDispatcher`: DOS, DPMI, timer, input, graphics, audio, and filesystem calls
* `TraceLogger`: loader decisions, HLE calls, exceptions, and execution milestones

## Milestones

1. Build a non-executing analysis tool.
2. Map the LE image into memory.
3. Build the minimal runtime shell.
4. Enter the entry point in a controlled way.
5. Implement minimal DOS/DPMI HLE from observed traces.
6. Connect graphics, input, and timing.
7. Generalize profiles for multiple versions.

## Immediate Next Task

First implement a tool that analyzes `MASTER\PIU_1ST\PIU.EXE` without executing it.

The tool should print the MZ/LE headers, object table, page table, fixup locations, and entry point.
