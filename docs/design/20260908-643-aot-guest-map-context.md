# Task 643 설계: AOT guest map 문맥 추적

## 한국어

### 문제

Task 642는 stale `PUSH EDI` 슬롯을 확인했지만, `0x010F1E56 RET` 직전의 명령과
ESP 조정은 아직 보이지 않습니다. 기존 `REPIU_AOT_GUEST_MAP_TRACE`는 정확히 지정한
주소만 출력하므로 인접 guest 명령 주소를 이미 알아야 합니다.

### 설계

`REPIU_AOT_GUEST_MAP_CONTEXT=<radius>`를 opt-in으로 추가합니다. 기존 map trace가
일치한 entry마다 address-map 순서의 앞뒤 `radius`개를 출력합니다. 각 문맥 행에는
guest/cache 주소, 길이, 활성 상태, 원본 guest bytes와 emitted cache bytes를 함께
둡니다. 반경은 과도한 출력을 막기 위해 64로 제한합니다.

설정이 없으면 기존 출력과 실행은 변하지 않습니다. `0x010F1E56` 문맥을 캡처하여
epilogue의 stack 조정과 최초 불일치 후보를 확인합니다.

## English

### Problem

Task 642 identified the stale `PUSH EDI` slot, but the instructions and ESP
adjustment immediately before `RET` at `0x010F1E56` remain hidden. Existing
`REPIU_AOT_GUEST_MAP_TRACE` prints only explicitly named addresses, requiring
neighbor addresses to be known in advance.

### Design

Add opt-in `REPIU_AOT_GUEST_MAP_CONTEXT=<radius>`. For each matched map entry,
print `radius` neighboring entries on each side in address-map order. Each row
includes guest/cache addresses, lengths, active state, original guest bytes,
and emitted cache bytes. Cap the radius at 64.

Without the setting, existing output and execution remain unchanged. Capture
the context of `0x010F1E56` to identify epilogue stack adjustment and the first
mismatch candidate.
