# Glide Drawing HLE Stubs 설계
# Glide Drawing HLE Stubs Design

## 개요 (Overview)

타이머 HLE 지원을 통해 락이 풀린 PIU.EXE 실행 흐름은 화면 렌더링 파이프라인으로 전입하여 Glide 그리기 관련 API들을 실시간으로 호출하기 시작했습니다.
최초로 관측된 API는 `_GRDRAWPOLYGON@12` (ordinal 76) 이며, 이와 밀접한 코어 Voodoo 3Dfx 그리기 API 5종을 함께 식별하였습니다.
이 5종의 그리기 API 메타데이터와 디스패처 스택 팝 처리를 HLE 계층에 추가하여 렌더링 파이프라인에서 발생하는 미등록 게이트 크래시를 미연에 원천 봉쇄하고, 최종적으로 `grDrawTriangle` 호출까지 안전하게 도달하도록 설계합니다.

After bypassing the spin lock via the Timer HLE, the execution flow of `PIU.EXE` moved into the rendering pipeline, invoking Glide drawing APIs.
The first observed API is `_GRDRAWPOLYGON@12` (ordinal 76). We identified five core drawing APIs related to this.
By registering their HLE metadata and dispatcher stack cleanup logic, we will prevent crashes inside the drawing loops and guarantee that the guest can safely call `grDrawTriangle`.

---

## 신설 대상 API 정의 및 스택 보정량 (API Specifications & Stack Correction)

스택 팝 크기는 `sizeof(Return_Address) + Argument_Bytes` 로 구성되며, 32비트 포인터 단위로 정렬되어 ESP에 직접 가산됩니다.

The stack cleanup size is calculated as `sizeof(Return_Address) + Argument_Bytes` and directly added to ESP in 32-bit dwords.

1. **`_GRDRAWPOINT@4` (ordinal 71):**
   * 인자 4바이트, 반환 `void`.
   * 스택 팝량: `4 + 4` = `8` 바이트 (`2U * sizeof(std::uint32_t)`)
2. **`_GRDRAWTRIANGLE@12` (ordinal 73):**
   * 인자 12바이트, 반환 `void`.
   * 스택 팝량: `4 + 12` = `16` 바이트 (`4U * sizeof(std::uint32_t)`)
3. **`_GRDRAWPLANARPOLYGON@12` (ordinal 74):**
   * 인자 12바이트, 반환 `void`.
   * 스택 팝량: `4 + 12` = `16` 바이트 (`4U * sizeof(std::uint32_t)`)
4. **`_GRDRAWPLANARPOLYGONVERTEXLIST@8` (ordinal 75):**
   * 인자 8바이트, 반환 `void`.
   * 스택 팝량: `4 + 8` = `12` 바이트 (`3U * sizeof(std::uint32_t)`)
5. **`_GRDRAWPOLYGON@12` (ordinal 76):**
   * 인자 12바이트, 반환 `void`.
   * 스택 팝량: `4 + 12` = `16` 바이트 (`4U * sizeof(std::uint32_t)`)

---

## 구현 상세 (Implementation Details)

### 1) Glide Signature 테이블 확장 (glide_hle.cpp)
* `kObservedSignatures` 배열의 크기를 기존 `26` 에서 `31` 로 변경하고, 위의 5개 API 시그니처 엔트리를 추가 정의합니다.

### 2) 디스패처 분기 추가 (execution_trampoline.cpp)
* `DispatchWin32GlideHle` 함수 꼬리부분에 각 API의 이름 매칭 분기문을 추가하여, `glide_gate_handled_count` 를 증가시키고 `Eip` 를 `return_address` 로 갱신하며 계산된 스택 팝 값을 `Esp` 에 더해준 뒤 `true` 를 반환하여 원활한 흐름 분기를 마무리합니다.
* 예시 (`_GRDRAWTRIANGLE@12`):
  ```cpp
      if (glide_export->name == "_GRDRAWTRIANGLE@12")
      {
          ++context->glide_gate_handled_count;
          win32_context->Eip = return_address;
          win32_context->Esp += 4U * sizeof(std::uint32_t);
          return true;
      }
  ```
