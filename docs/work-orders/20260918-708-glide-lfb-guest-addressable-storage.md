# Task 708 작업 지시 — Glide LFB staging surface의 32비트 주소 보장

설계: [20260918-708](../design/20260918-708-glide-lfb-guest-addressable-storage.md)

## 범위

게스트가 `grLfbLock`으로 받는 `lfbPtr`이 4 GiB 아래를 가리키도록 만든다.
게스트 코드, Glide semantics, arena 배치, LFB 픽셀 변환은 건드리지 않는다.

## 단계

1. 공용 낮은 주소 예약
   * `include/repiu/runtime/low_address_reservation.h`
   * `src/runtime/low_address_reservation.cpp`
   * 후보 목록, 용량, 64비트 host 무힌트 최후수단 허용 여부를 받는다.
   * 적합성은 마지막 바이트까지 본다.
   * 결과에 `fits_32bit`를 둔다.
2. 기존 두 소비자를 공용 단위 위로 옮긴다. 공개 API, 후보 목록, 메시지
   문자열, 최후수단 정책은 그대로 둔다.
   * `src/runtime/aot_code_cache_reservation.cpp`
   * `src/runtime/aot_shadow_selector_block.cpp`
3. `GlideLfbSurface::UseExternalStorage(std::uint8_t*, std::size_t)`를 더한다.
   `Resize`는 설치된 저장소를 쓰고 들어가지 않는 크기를 거부한다. 설치되지
   않으면 기존 `std::vector` 경로 그대로다. `repiu::hle`에 새 의존성을
   들이지 않는다.
4. `src/engine/boundary/glide_lfb_guest_storage.{h,cpp}`를 새로 만든다.
   예약 하나를 소유하고 surface에 설치하고 해제한다.
5. `ThreadContext`에 멤버를 더하고 소멸자에서 해제한다. 멤버는
   `glide_lfb_surface`보다 먼저 선언한다. 설계 257 §3.1을 인용하는 기존
   주석에 32비트 주소 조건을 덧붙인다.
6. `linexe_glide_boundary.cpp`의 두 `Resize` 호출 앞에 멱등한 설치 호출을
   둔다. 통합 지점에는 adapter 코드만 남긴다.
7. CMake에 새 source를 등록한다.
8. probe
   * `low_address_reservation` probe 신규, core probe 목록에 등록
   * `glide_lfb_region` probe에 외부 저장소 사례 추가
9. 문서: 작업 로그, `docs/analysis/linux-port-frontier.md`, `ARCHITECTURE.md`의
   해당 절.

## 검증

* Linux x64 Debug `repiu`, `repiu_core_probe` 빌드와 core probe 전체
* Win32 x86 Debug 전체 빌드와 `repiu_core_probe`
* 실제 `pumpit2a` 30초 실행

## 완료 조건

* `grLfbLock`이 보고하는 `lfbPtr`이 4 GiB 아래다.
* 실행이 `access=0x...` SIGSEGV 없이 예산을 채운다.
* 새 probe가 수정 전 구현에서 실패하고 수정 후 통과한다.
* 기존 group이 하나도 회귀하지 않는다.

## 완료 조건이 아닌 것

LFB에 기록된 내용이 화면에 옳게 나타나는지는 이 작업의 판정 대상이 아니다.

---

## English

Design: [20260918-708](../design/20260918-708-glide-lfb-guest-addressable-storage.md)

### Scope

Make the `lfbPtr` the guest receives from `grLfbLock` point below 4 GiB. Guest
code, Glide semantics, arena placement and LFB pixel conversion are untouched.

### Steps

1. Add the shared low-address reservation
   (`include/repiu/runtime/low_address_reservation.h`,
   `src/runtime/low_address_reservation.cpp`): a request carrying the candidate
   list, the capacity and whether a 64-bit host may fall back to an unhinted
   request; a fit test that reaches the last byte; a `fits_32bit` result.
2. Move `aot_code_cache_reservation.cpp` and `aot_shadow_selector_block.cpp`
   onto it, leaving their public APIs, candidate lists, message strings and
   last-resort policies unchanged.
3. Add `GlideLfbSurface::UseExternalStorage(std::uint8_t*, std::size_t)`.
   `Resize` then uses the installed storage and refuses a size that does not
   fit; without it the existing `std::vector` path is unchanged. Add no new
   dependency to `repiu::hle`.
4. Add `src/engine/boundary/glide_lfb_guest_storage.{h,cpp}`, owning one
   reservation, installing it into the surface, and releasing it.
5. Give `ThreadContext` the member, release it in the destructor, and declare
   it before `glide_lfb_surface`. Extend the existing comment citing design
   257 §3.1 with the 32-bit address condition.
6. Put one idempotent installation call ahead of each of the two `Resize` call
   sites in `linexe_glide_boundary.cpp`, leaving only adapter code there.
7. Register the new sources with CMake.
8. Probes: a new `low_address_reservation` probe registered in the core-probe
   list, and an external-storage case added to the `glide_lfb_region` probe.
9. Documents: the work log, `docs/analysis/linux-port-frontier.md`, and the
   relevant section of `ARCHITECTURE.md`.

### Verification

* Linux x64 Debug `repiu` and `repiu_core_probe` builds and every core-probe
  group
* Win32 x86 Debug full build and `repiu_core_probe`
* A real 30-second `pumpit2a` run

### Done when

* The `lfbPtr` reported by `grLfbLock` is below 4 GiB.
* The run reaches its budget with no `access=0x...` SIGSEGV.
* The new probe fails against the pre-fix implementation and passes after it.
* No existing group regresses.

### Not a completion condition

Whether what the guest writes into the LFB then appears correctly on screen is
not judged by this task.
