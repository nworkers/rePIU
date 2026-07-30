# 20260730-365 동일 Glide 상태 생략 작업 지시 / Work order

* 설계: [20260730-365-glide-setter-state-elision.md](../design/20260730-365-glide-setter-state-elision.md)
* 근거 측정: [20260730-364-glide-setter-state-census.md](../work-logs/20260730-364-glide-setter-state-census.md)
* 범위: batch 1의 7종 setter만. 기본 ON, `REPIU_GLIDE_SETTER_ELIDE=0`으로 복원.

## 한국어

### 1. 구현 항목

| # | 파일 | 내용 |
|---|---|---|
| 1 | `include/repiu/platform/win32/glide_setter_state_model.h` | 공유 rule: key 자료형·동등성·gate 분류 (census에서 추출) |
| 2 | `src/platform/win32/telemetry/glide_setter_state_model.cpp` | 위 구현 |
| 3 | `include/repiu/platform/win32/glide_setter_state_census.h` | 추출된 rule 제거, model 포함 |
| 4 | `src/platform/win32/telemetry/glide_setter_state_census.cpp` | 동일 |
| 5 | `include/repiu/platform/win32/glide_setter_state_cache.h` | applied cache, 생략 판정, batch 1 목록, 계수 |
| 6 | `src/platform/win32/telemetry/glide_setter_state_cache.cpp` | 위 구현 (`REPIU_GLIDE_SETTER_ELIDE`) |
| 7 | `src/platform/win32/execution/thread_context.h` | cache 보유 |
| 8 | `src/platform/win32/boundary/linexe_glide_boundary.cpp` | census scope를 census+cache 통합 scope로 확장, ABI 검증 뒤 생략 분기 |
| 9 | `include/repiu/platform/win32/execution_trampoline.h` | cache 스냅샷 필드 |
| 10 | `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 스냅샷 수집 |
| 11 | `src/host/win32/main.cpp` | 종료 요약과 ordinal별 생략 계수 |
| 12 | `src/tools/aot_probe/glide_setter_state_cache_probe.{h,cpp}` | 단위 probe |
| 13 | `src/tools/aot_probe/main.cpp`, `CMakeLists.txt` | probe 등록과 빌드 |
| 14 | `scripts/task365_glide_setter_state_elision.ps1` | 3회 OFF/ON A/B와 gate E1~E8 |

### 2. 필수 제약

* **규칙을 두 번 정의하지 않습니다.** key 생성, 동등성, gate 분류(대상/무효화/texture
  generation/texture 의존)는 model 모듈에만 존재하고 census와 cache가 함께 씁니다.
* 생략 판정은 반환 주소·signature·인수 크기 검증을 **모두 통과한 뒤**에만 적용합니다.
* 생략 경로는 `backend.message_`와 `context->glide_backend_message`를 건드리지
  않습니다(설계 §3.4).
* 생략 시 `++glide_gate_handled_count`, `Eip = return_address`,
  `Esp += sizeof(uint32) + argument_byte_count`를 기존 case와 동일하게 수행합니다.
  batch 1은 전부 void 반환이므로 `Eax`는 건드리지 않습니다.
* batch 1 목록은 census 대상 목록의 **부분집합**이어야 합니다. probe가 이를 검사합니다.
* hot path에서 allocation, 문자열 생성, 정렬을 하지 않습니다.
* census는 순수 관측자로 남습니다. 생략된 호출도 census는 `same`으로 계수해 OFF/ON
  비교가 가능해야 합니다(gate E1의 근거).

### 3. 검증 절차

1. `scripts/build_win32_x86.bat` (Debug) 통과
2. `scripts/build_win32_x86_release.bat` (Release) 통과
3. `repiu_aot_probe.exe` exit 0 — 두 구성, 신규 probe 포함
4. `scripts/task365_glide_setter_state_elision.ps1 -Runs 3 -DurationSeconds 60`
   * 설계 §6의 E1~E8을 script가 검사하고 위반 시 throw
   * **E1(census `same` == cache `elided`)은 실패 시 즉시 되돌림 사유**
5. `REPIU_GLIDE_FRAME_DUMP`로 양쪽 BMP를 남기고 육안 비교(E7)
6. 설계 §7의 P1~P4 중 하나를 판정해 작업 로그에 기록

### 4. 완료 조건

* 두 구성 빌드와 probe 통과
* E1~E6 통과, E7 육안 비교 결과 기록
* P1~P4 판정과 다음 batch 여부가 문서에 기록됨
* 원본 EXE·게임 로직 변경 없음이 diff로 확인됨

---

## English

### Scope

Batch one only: the seven setters Task 364 measured at 99.9%-or-better repetition
with one or two distinct values. Elision is on by default with
`REPIU_GLIDE_SETTER_ELIDE=0` restoring the rendezvous.

The shared rules — key construction, equality, and gate classification — move out
of the census into a model module so the observer and the actor cannot diverge.
The new cache module owns the applied-state records, the batch-one list, and the
elision decision. The boundary's existing census scope becomes one combined
scope, so the key is built once and the outcome classified once for both
consumers.

### Constraints

The elision decision applies only after return-address, signature, and
argument-size validation pass. The elided path leaves both the host-owned
`backend.message_` and the guest-side diagnostic mirror untouched. It performs
the same `handled_count` increment, return-address restore, and stdcall cleanup
the case bodies do, and touches no `Eax` because all seven return void. The
batch-one list must be a subset of the census target list, which the probe
checks. The census stays a pure observer and still counts an elided call as
`same`, which is what makes gate E1 possible. No allocation, string building, or
sorting on the hot path.

### Verification

Both builds and the probe suite must pass, then
`scripts/task365_glide_setter_state_elision.ps1 -Runs 3 -DurationSeconds 60`
must satisfy gates E1 through E8, throwing on violation. **A failure of E1 — the
pure observer's `same_count` not equalling the actor's `elided_count` — is grounds
for immediate revert**, because it means the decision and the observation
disagree. Frame dumps are captured in both configurations for the human
comparison in E7. The run then decides one of P1 through P4, recorded in the work
log along with whether batch two proceeds.
