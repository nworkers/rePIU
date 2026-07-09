# DS 메모리 읽기 HLE 작업 지시

## 목표

`piu_1st` 실행 중 `8B 06`에서 발생하는 중단을 segment HLE 경로에서 처리하여 다음 실행 지점까지 진행할 수 있게 한다.

## 작업 범위

* `8B 06` / `mov eax, dword ptr ds:[esi]` 형태의 관측된 DS dword read를 처리한다.
* `DS` shadow selector가 존재하고 `ESI=0`인 경우 `EAX=0`으로 응답한다.
* `8B 06` 처리 후 관측된 `80 3E 00`, `AC`, `A4` low-memory byte read/copy를 `DS` shadow selector와 `ESI < 0x10000` 조건에서 0으로 처리한다.
* segment memory load 기록에 width를 추가한다.
* loader 로그에서 segment memory load width와 폭에 맞는 value를 출력한다.
* 작업 결과와 다음 정지점을 작업 로그에 남긴다.

## 제외 범위

* 범용 DPMI descriptor table 해석은 구현하지 않는다.
* 모든 DS 기반 메모리 접근을 일반화하지 않는다.
* 기존 DOS 콘솔 샘플용 `HandleDosMemoryAccess` fallback은 이번 작업에서 제거하지 않는다.

## 검증

* `scripts\test_all.ps1`을 실행한다.
* 가능하면 `piu_1st`를 직접 실행해 `8B 06` 정지점이 segment HLE로 처리되는지 확인한다.
* OpenWatcom 샘플 baseline 비교는 변경 영향이 의심되면 추가로 실행한다.

# DS Memory Read HLE Work Order

## Goal

Handle the `8B 06` stop observed during `piu_1st` execution through the segment HLE path so execution can proceed to the next point.

## Scope

* Handle the observed DS dword read form, `8B 06` / `mov eax, dword ptr ds:[esi]`.
* Return `EAX=0` when a DS shadow selector exists and `ESI=0`.
* Handle the `80 3E 00`, `AC`, and `A4` low-memory byte read/copy forms observed after `8B 06` as zero when a DS shadow selector exists and `ESI < 0x10000`.
* Add width to segment memory load records.
* Log the segment memory load width and a width-appropriate value in the loader.
* Record the result and next stop in the work log.

## Out of Scope

* Do not implement general DPMI descriptor table interpretation.
* Do not generalize all DS-based memory accesses.
* Do not remove the existing DOS console sample `HandleDosMemoryAccess` fallback in this task.

## Verification

* Run `scripts\test_all.ps1`.
* If possible, run `piu_1st` directly and confirm that the `8B 06` stop is handled by segment HLE.
* Run the OpenWatcom sample baseline comparison if the change appears to affect that path.
