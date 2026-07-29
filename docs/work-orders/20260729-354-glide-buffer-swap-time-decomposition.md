# 20260729-354 Glide buffer swap 시간 분해 작업 지시 / Work order

* 설계: [20260729-354-glide-buffer-swap-time-decomposition.md](../design/20260729-354-glide-buffer-swap-time-decomposition.md)

## 한국어

### 목표

`grBufferSwap` host work를 setup/present/accounting/finalize로 분해하고 실제 SDL
swap interval을 관측해 다음 최적화 또는 충실도 검증 대상을 확정합니다.

### 구현

1. 기본 OFF인 backend-owned swap timing profile과 snapshot을 추가합니다.
2. `BufferSwap`의 host-side 정상·실패 경로에 phase timestamp와 요청 interval 기록을
   연결합니다.
3. 첫 profile swap에서 `SDL_GL_GetSwapInterval`을 관측하되 설정은 변경하지 않습니다.
4. 최종 execution attempt와 loader summary에 snapshot을 연결합니다.
5. 합성 probe로 정책, phase 합, clamp, interval, query, inert 경로를 검증합니다.
6. Task 347을 재사용하는 control/profile 3회 측정 wrapper와 CSV/JSON 출력을
   추가합니다.

### 완료 조건

* 설계 G1~G7을 코드와 wrapper가 검증합니다.
* Win32 x86 Release loader와 AOT probe가 빌드되고 전체 probe가 성공합니다.
* 짧은 smoke에서 출력·count·coverage를 확인한 뒤 60초 3회 측정을 완료합니다.
* 결과와 다음 판단을 analysis, architecture, work log에 한국어/영어로 반영합니다.
* 변경을 하나의 작업 단위 Git 커밋으로 남깁니다.

---

## English

### Objective and implementation

Decompose `grBufferSwap` host work into setup, present, accounting, and
finalize phases and observe the actual SDL swap interval without changing it.

Add a disabled-by-default backend-owned profile and snapshot, instrument the
host-side success and failure paths, query `SDL_GL_GetSwapInterval` once,
connect final reporting, add synthetic policy/aggregation/clamp/interval/query
coverage, and add a three-run Release wrapper around Task 347.

### Completion

Enforce design gates G1--G7, build the Win32 x86 Release loader and probe, pass
the full probe, complete smoke and three-run 60-second measurements, update
analysis, architecture, and the bilingual work log, and commit the task as one
unit.
