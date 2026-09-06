# Task 611 작업 지시 — DOS/4GW memory path probe

## 한국어

### 작업 순서

1. `AX=3000h` HLE 처리에 `REPIU_DOS4GW_MEMORY_PATH_PROBE=pharlap` opt-in
   분기를 추가한다.
2. 기존 low word `0x0007`을 보존하고 high word `0x4458`만 probe에서
   설정한다. 기본값과 알 수 없는 환경변수 값은 변경하지 않는다.
3. DOS trace에 probe 활성 여부와 `AX=3000h` 반환 EAX를 기록한다.
4. `repiu_core_probe`를 빌드·실행한다.
5. 기본 `pumpit2a`와 probe `pumpit2a`를 각각 실행해 `AH=4Ah`, allocator
   상태, 종료 방식, fault 여부를 비교한다.
6. `pumpit2a` 결과를 `docs/analysis/linux-port-frontier.md`에 누적하고
   설계/작업 로그를 갱신한다.
7. probe가 memory contract를 설명하지 못하면 영구 HLE로 승격하지 않는다.

### 완료 조건

* 기본 `AX=3000h` 응답이 그대로 유지된다.
* probe 실행에서 반환 high word와 실제 원본 분기가 로그로 확인된다.
* `AH=4Ah` 도달 여부가 확정된다.
* core probe `24/24`가 유지된다.
* 임의 guest EIP 우회나 allocator metadata injection이 없다.

### 실행 결과

probe 실행에서 `AX=44580007h`와 원본 `AH=4Ah`가 확인됐다. resize는
성공 반환했지만, 이후에도 동일한 file-structure 오류와 `AX=4C01h`
종료가 발생했다. 따라서 probe는 분기 확인에는 성공했으나 memory
contract 해결책으로 승격하지 않는다. 다음 작업은 resize 이후 allocator가
왜 0을 유지하는지 원본 call/return과 전역 상태를 추적하는 것이다.

### 금지 사항

* 기본 실행에 PharLap signature를 강제하지 않는다.
* 관찰되지 않은 `AH=4Ah` 성공을 구현하지 않는다.
* stack top, selector limit, free-list를 추측으로 변경하지 않는다.

## English

### Sequence

1. Add an opt-in `REPIU_DOS4GW_MEMORY_PATH_PROBE=pharlap` branch to the
   `AX=3000h` HLE path.
2. Preserve low word `0x0007` and set only high word `0x4458` under the probe;
   leave unset and unknown values unchanged.
3. Record probe activation and returned EAX in the DOS trace.
4. Build and run `repiu_core_probe`.
5. Run default and probe `pumpit2a` and compare `AH=4Ah`, allocator state,
   termination, and faults.
6. Append the result to `docs/analysis/linux-port-frontier.md` and update the
   design/work-log documents.
7. Do not promote the probe to permanent HLE if it does not explain the memory
   contract.

### Done criteria

* The default `AX=3000h` response remains unchanged.
* The probe return high word and actual original branch are observable.
* Whether execution reaches `AH=4Ah` is decided.
* Core probe remains `24/24`.
* No guest-EIP bypass or allocator metadata injection is used.

### Run result

The probe confirmed `AX=44580007h` and the original `AH=4Ah`. Resize returned
success, but the same file-structure error and `AX=4C01h` termination followed.
The probe therefore confirms branch selection only and is not promoted as the
memory-contract solution. The next task will trace why the allocator remains
zero after resize, using original call/return flow and global state.

### Prohibitions

* Do not force the PharLap signature in the default run.
* Do not implement unobserved `AH=4Ah` success.
* Do not guessingly change stack top, selector limits, or free-list state.
