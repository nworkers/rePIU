# Task 707 작업 지시 — Linux x64 x87 tag word 변환

설계: [20260918-707](../design/20260918-707-linux-x64-x87-tag-word-conversion.md)

## 범위

Linux x86-64 signal 문맥과 `GuestCpuContext` 사이의 x87 tag word 변환만
고친다. 게스트 코드, AOT 하강, Glide 경계, i386 경로는 건드리지 않는다.

## 단계

1. `src/platform/linux/guest_cpu_context.cpp`
   * `ClassifyFloatingTag`가 지수에서 부호 비트를 제거하고 판정하도록
     정정한다.
   * x86-64 `LoadFloatingSave`가 abridged `ftw` 비트를 먼저 보고, 비어
     있는 물리 레지스터를 `0x03`으로 확장하도록 고친다. 사용 중인
     레지스터만 `_st[(j - TOP) & 7]`을 분류한다.
   * x86-64 `StoreFloatingSave`가 tag `0x03`인 물리 레지스터의 `ftw`
     비트만 0으로 두도록 고친다.
2. `src/tools/aot_probe/guest_cpu_context_probe.cpp`
   * x86-64 round trip도 `TagWord`를 포함해 비교하도록 바꾼다.
   * `TOP = 3`, 물리 레지스터별 tag가 서로 다른 사례를 추가한다. 기대값은
     `TagWord = 0xFF27`, 저장된 `ftw = 0x0E`이다.
3. `docs/kb/x87-state-and-tag-words.md`를 새로 쓰고
   `docs/kb/README.md` 색인을 갱신한다.
4. `docs/analysis/linux-port-frontier.md`에 Task 707 절을 덧붙인다.
5. 작업 로그를 남긴다.

## 검증

* Linux x64 Debug `repiu`, `repiu_core_probe` 빌드
* Linux x64 core probe 전체 group
* 실제 `pumpit2a` 30초 실행에서 첫 삼각형 정점 값 확인
* Win32 x86 Debug 빌드와 `repiu_core_probe`

## 완료 조건

* 첫 삼각형 세 정점의 x/y가 NaN이 아니다.
* 새 probe 사례가 수정 전 구현에서 실패하고 수정 후 통과한다.
* 기존 group이 하나도 회귀하지 않는다.

---

## English

Design: [20260918-707](../design/20260918-707-linux-x64-x87-tag-word-conversion.md)

### Scope

Only the x87 tag-word conversion between the Linux x86-64 signal context and
`GuestCpuContext`. Guest code, AOT lowering, the Glide boundary, and the i386
path are untouched.

### Steps

1. `src/platform/linux/guest_cpu_context.cpp`: mask the sign bit out of the
   exponent in `ClassifyFloatingTag`; make x86-64 `LoadFloatingSave` read the
   abridged `ftw` first and expand an empty physical register to `0x03`,
   classifying `_st[(j - TOP) & 7]` only for registers in use; make x86-64
   `StoreFloatingSave` clear the `ftw` bit only for a tag of `0x03`.
2. `src/tools/aot_probe/guest_cpu_context_probe.cpp`: compare `TagWord` on
   x86-64 too, and add a case with `TOP = 3` and a different tag per physical
   register, expecting `TagWord = 0xFF27` and a stored `ftw = 0x0E`.
3. Add `docs/kb/x87-state-and-tag-words.md` and update `docs/kb/README.md`.
4. Append a Task 707 section to `docs/analysis/linux-port-frontier.md`.
5. Write the work log.

### Verification

* Linux x64 Debug `repiu` and `repiu_core_probe` builds
* Every Linux x64 core-probe group
* The first triangle's vertex values on a real 30-second `pumpit2a` run
* Win32 x86 Debug build and `repiu_core_probe`

### Done when

* The x/y of all three first-triangle vertices are not NaN.
* The new probe case fails against the pre-fix implementation and passes
  after it.
* No existing group regresses.
