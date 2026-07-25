# 20260725-295 AOT HLE 트랩 주소 변환 작업 지시서 / AOT HLE trap address translation work order

## 한국어

### 목표

AOT DBT 모드(`aot-dbt`) 실행 중에 발생하는 하드웨어 보호 예외(General Protection Fault 등)의 EIP 주소가 AOT 코드 캐시 내에 위치하더라도, 이를 게스트 EIP 주소로 변환하여 기존 HLE 예외 처리 핸들러 체인을 통과시키고 예외를 정상적으로 처리하여 흐름이 이어지게 합니다.

### 작업 범위

- [ ] `DispatchGuestException` 시작 시점에 `win32_context->Eip`가 AOT 코드 캐시 영역인지 판별하는 조건문 추가.
- [ ] AOT 주소인 경우 `FindAotGuestAddress`를 사용해 게스트 EIP 주소로 변환하고 일시적으로 `win32_context->Eip`를 패치하는 로직 구현.
- [ ] HLE 핸들러(특권 명령어, 포트 I/O, DPMI 인터럽트, 세그먼트 로드 등)의 반환값 감지.
- [ ] HLE 핸들러가 예외 처리에 성공한 경우(`true` 반환), 전진된 게스트 EIP를 `FindAotCacheAddress`로 역변환하여 `win32_context->Eip`를 새로운 AOT 캐시 주소로 업데이트하는 로직 구현.
- [ ] 역변환 실패 시 게스트 EIP를 그대로 유지하여 컴파일/DBT 가드 동작 유도.
- [ ] HLE 핸들러가 실패한 경우(`false` 반환), `win32_context->Eip`를 원래의 AOT 캐시 주소로 롤백.
- [ ] `REPIU_EXECUTION_BACKEND=aot-dbt` 환경변수를 활성화한 빌드로 테스트하여 예외 지점에서 정상적으로 HLE 동작이 재개되는지 확인.
- [ ] 60초/20초 실행 시 Glide LFB 덤프 획득 및 검증.

### 범위 밖

- AOT 컴파일 단계에서의 개별 어셈블리 변환 방식 수정.
- 게스트 메모리 보호 수준 및 가드 페이지 정책의 변경.

### 완료 조건

- AOT 모드 기동 시 첫 `STI` 명령어에서 발생하는 `0xC0000096` 예외가 정상 캡처되고 롤백/Teardown 없이 HLE 처리되어 전진해야 함.
- 20초 이내에 Glide LFB Lock/Unlock이 호출되어 비트맵 덤프 파일이 `build/texture_dumps/`에 성공적으로 떨어져야 함.
- 빌드가 정상적으로 완료되고 예외 주소 변환 과정에서 Double Fault/Access Violation으로 인한 프로세스 즉시 크래시가 발생하지 않아야 함.

---

## English

### Goal and Scope

Translate the EIP of hardware protection exceptions (such as GPF) occurring inside the AOT code cache during AOT DBT mode (`aot-dbt`) to the corresponding guest EIP, allowing the existing HLE handler chain to process the exception and continue execution.

- Implement condition checks at the start of `DispatchGuestException` to detect AOT cache EIP.
- Translate AOT EIP to guest EIP using `FindAotGuestAddress` and temporarily patch `win32_context->Eip`.
- Capture HLE handler results (privileged traps, Port I/O, DPMI, segment loads, etc.).
- If handled successfully (returns `true`), reverse-translate the advanced guest EIP back to the AOT cache address using `FindAotCacheAddress` and set `win32_context->Eip`.
- If reverse translation fails, keep the guest EIP to invoke the DBT page-fault fallback compilations.
- If unhandled (returns `false`), rollback `win32_context->Eip` to the original AOT cache address.
- Verify running under `REPIU_EXECUTION_BACKEND=aot-dbt` environment, ensuring execution bypasses STI/CLI exceptions and progresses successfully.
- Verify Glide LFB bitmap dump acquisition within 20s.

### Out of Scope

- Modifying the translation behavior of individual assembly instructions inside the AOT compiler.
- Modifying the guest memory page protection settings or guard policies.

### Completion Criteria

- The `0xC0000096` exception raised by the first `STI` in AOT mode must be captured, handled via HLE, and advanced without triggering process teardown.
- Glide LFB Lock/Unlock must be hit within 20 seconds, producing bitmap dumps under `build/texture_dumps/`.
- The project builds cleanly, and the exception translation must not cause secondary crashes (Double Faults or host Access Violations).
