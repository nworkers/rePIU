# Task 699 작업 로그 — Linux x64 dynamic-only execution probe arming

## 결과

브랜치 업데이트 뒤 미완료 상태였던 dynamic-only probe arming 구현을 빌드하고 실제
실행으로 확인했습니다. 초기 map miss는 더 이상 configuration을 해제하지 않으며,
dynamic append 이후 기존 설치 경로가 `0x010F928B` sentinel을 generation 9에
설치했습니다. AOT breakpoint 역변환 직후 snapshot 기록 호출도 빌드에 포함됐습니다.

## 검증

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 통과
* Linux x64 Debug `repiu`: 빌드 성공
* 실제 `pumpit2a` 두 번에서 다음 설치 line 확인:

```text
[repiu-aot-probe] dynamic guest=0x010F928B generation=9 added_bytes=6259 installed=1
```

두 실행은 대상 entry를 실행하기 전에 guest `0x010F777C`, cache `0x200695A2`의
planner-HLE 경계에서 fail-closed SIGTRAP으로 끝났습니다. 따라서 configured 상태와
dynamic 설치는 확인했지만 hit 및 register snapshot은 아직 확인하지 못했습니다.
다음 작업에서 `67 0F B6 50 01`의 16-bit addressing byte load를 처리한 뒤 이 검증을
재개해야 합니다.

## English

### Result

After the branch update, built and exercised the incomplete dynamic-only probe
arming change. An initial-map miss no longer clears configuration, and the
existing post-append path installed the `0x010F928B` sentinel in generation 9.
The build also includes snapshot recording immediately after AOT-breakpoint
reverse mapping.

### Verification

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 passed
* Linux x64 Debug `repiu`: build passed
* Two real `pumpit2a` runs printed the dynamic installation line above.

Both runs ended at the fail-closed planner-HLE boundary for guest `0x010F777C`
and cache `0x200695A2` before executing the target entry. Configuration and
dynamic installation are therefore confirmed, but the hit and register
snapshot are not. The next task must handle the 16-bit-addressing byte load
`67 0F B6 50 01`, then resume this verification.
