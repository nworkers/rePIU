# Task 494 작업 지시: 2P 숫자패드 별칭

## 작업 항목

- [x] SDL mapping에 `KP_7/KP_9/KP_5/KP_1/KP_3`과 navigation 별칭을 함께 둡니다.
- [x] Win32 초기 pressed mask에 `VK_NUMPAD7/9/5/1/3`을 추가합니다.
- [x] replay 밖 live scan도 두 virtual-key 계열을 모두 읽게 합니다.
- [x] timeline probe와 전체 AOT probe, Win32 Debug 빌드를 수행합니다.
- [x] analysis, architecture, TODO와 작업 로그를 갱신합니다.

## 완료 조건

자동 검증이 통과하고 NumLock 상태와 무관한 2P 다섯 위치가 사용자 실행에서 확인되어야
합니다. Task 495 최종 실행에서 다섯 위치의 press/release가 모두 균형을 이뤄 충족했습니다.

---

# Task 494 Work Order: 2P Numpad Aliases

## Work Items

- [x] Add both keypad and navigation aliases to the SDL mapping.
- [x] Add `VK_NUMPAD7/9/5/1/3` to initial Win32 pressed-mask capture.
- [x] Make the live scan outside replay query both virtual-key families.
- [x] Run the timeline probe, complete AOT probe, and Win32 Debug builds.
- [x] Update analysis, architecture, TODO, and the work log.

## Completion Criteria

Automated verification passes and a user run confirms all five 2P positions regardless of NumLock
state. The final Task 495 run satisfied this with balanced press/release counts for every position.
