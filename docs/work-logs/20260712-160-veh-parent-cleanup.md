# VEH parent cleanup 작업 로그 / Work Log

## 한국어

VEH 제거를 worker 내부에서 parent join 이후로 이동했습니다. 복귀 시 TF/DF와 selector를 복원하도록 상태를 확장했습니다. WGL의 `0x406D1388`을 통과한 뒤 guest는 Glide ordinal `0x5E`까지 진행하지만, `INT 21h/AH=4Ch` 이후 host DS 복귀는 아직 불완전합니다. 상태 구조를 읽는 recovery stub의 주소 공간 계약을 다음 결정에서 확정해야 합니다.

## English

Moved VEH removal from the worker to the parent after join and expanded recovery state for TF/DF and selectors. After passing WGL exception `0x406D1388`, the guest reaches Glide ordinal `0x5E`; however, host DS recovery after `INT 21h/AH=4Ch` remains incomplete. The next decision must settle the recovery stub's address-space contract.
