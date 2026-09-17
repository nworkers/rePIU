# 작업 지시 20260914-679 — Linux x64 혼합 모드 게스트 디코드

## 배경

Task 678의 shutdown recovery 수정 후에도 runtime coredump가 남았습니다. 재현
로그와 LE object flags를 대조한 결과, object 3의 16-bit code를 AOT planner가
32-bit로 디코드하여 가짜 `ESP=0x8DFB2000`을 생성한 것이 확인되었습니다.

## 작업 범위

* AOT instruction record에 guest code mode를 추가합니다.
* static planner의 decoder를 executable object mode에 맞게 선택합니다.
* long-mode classifier/lowerer가 record mode를 사용하도록 연결합니다.
* dynamic append가 초기 image의 executable object mode 범위를 재사용하도록
  metadata를 전달합니다.
* 미지원 16-bit form은 32-bit로 잘못 방출하지 않고 HLE boundary로 닫습니다.
* 특정 guest 주소나 특정 immediate를 조건으로 삼는 예외처리는 추가하지
  않습니다.

## 구현 순서

1. [ ] 설계 문서와 기존 mixed-mode/far-return 문서를 대조합니다.
2. [ ] mode metadata 구조와 planner mode-aware decode를 구현합니다.
3. [ ] long-mode classifier/lowerer의 mode 입력을 연결하고 안전한 거부 정책을
       적용합니다.
4. [ ] dynamic append에 mode range를 전달합니다.
5. [ ] core probe에 16-bit decode/lowering regression을 추가합니다.
6. [ ] Linux x64 target build와 core probe를 실행합니다.
7. [ ] WSL runtime smoke와 fault 로그를 확인합니다.
8. [ ] 관련 analysis/work-log 문서를 갱신하고 커밋합니다.

## 최소 검증

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j2
./build/linux_x64_debug/repiu_core_probe
timeout 6s ./build/linux_x64_debug/repiu pumpit2a
```

WSL에서는 저장소 경로를 `/mnt/e/MYWORK/Projects/rePIU`로 사용하고, runtime
smoke 결과의 종료 신호·fault EIP·AOT trace를 작업 로그에 기록합니다.

---

# Work Order 20260914-679 — Linux x64 mixed-mode guest decoding

## Background

The runtime coredump remained after Task 678's shutdown-recovery fix. Comparing
the reproduction log with the LE object flags confirmed that AOT decoded object
3's 16-bit code as 32-bit and created the fake `ESP=0x8DFB2000`.

## Scope

* Add guest code mode to the AOT instruction record.
* Select the static planner decoder from the executable object's code mode.
* Connect the recorded mode to long-mode classification/lowering.
* Carry executable-object mode ranges into dynamic append.
* Close unsupported 16-bit forms at an HLE boundary rather than emitting them
  as 32-bit instructions.
* Do not add conditions keyed to a particular guest address or immediate.

## Implementation order

1. [ ] Compare the design with the existing mixed-mode/far-return documents.
2. [ ] Implement mode metadata and mode-aware planner decoding.
3. [ ] Connect mode to long-mode classification/lowering with safe refusal.
4. [ ] Pass mode ranges through dynamic append.
5. [ ] Add a 16-bit decode/lowering regression to the core probe.
6. [ ] Build the Linux x64 target and core probe.
7. [ ] Run the WSL runtime smoke and inspect fault logs.
8. [ ] Update the related analysis/work-log documents and commit.

## Minimum verification

Use the commands shown in the Korean section, adapting the executable path to
the WSL mount. Record the smoke exit signal, fault EIP, and AOT trace in the work
log.

## 현재 체크포인트

구현과 빌드 검증까지 진행하고 이 작업 단위를 중단한다. 16-bit lowering/HLE와
실제 게임 재실행은 다음 세션의 후속 범위로 남긴다. 현재 작업 브랜치의 전체
변경은 커밋하여 다음 세션에서 그대로 재개할 수 있도록 보존한다.

## Current checkpoint

Implementation and build verification are recorded, and this work unit stops at
this checkpoint. 16-bit lowering/HLE and a new full-game run remain follow-up
work for the next session. All current branch changes are committed so the next
session can resume from this exact state.
