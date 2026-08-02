# 20260802-395 AOT-DBT 미해결 direct-edge dispatch 작업 로그

설계: [20260802-395-aot-dbt-unresolved-direct-edge-dispatch.md](../design/20260802-395-aot-dbt-unresolved-direct-edge-dispatch.md)  
작업 지시: [20260802-395-aot-dbt-unresolved-direct-edge-dispatch.md](../work-orders/20260802-395-aot-dbt-unresolved-direct-edge-dispatch.md)

## 한국어

### 원인 확인

pumpit2의 `0x010FB9D1` direct call은 `0x010EFE34`의 return-address-consuming thunk를
호출합니다. planner가 보수적으로 return address `0x010FB9D6`도 reachable로 처리하면서
실제로는 데이터인 바이트를 해석했고, decode가 멈춘 뒤 `0x010FB9E5 -> 0x010FB9E6`
`kBlockFallthrough` fixup 한 건이 cache 밖에 남았습니다. 기존 builder가 완성 image의
미해결 direct edge를 거부하므로 모든 AOT-family backend가 실행 전 실패했습니다.

### 구현

- 공용 code-cache image에 AOT-DBT direct-edge dispatch option과 image-relative site
  metadata를 추가했습니다.
- 미해결 direct call/jump/conditional/fall-through fixup만 21바이트 tail stub로
  연결했습니다. 정상 mapped edge는 기존 `rel32`를 유지합니다.
- Win32 x86 host-stack thunk는 site/target을 검증하고 공용
  `ResolveAotTransferTarget`을 호출합니다. 실패 시 site-owned INT3로 돌아갑니다.
- static placement, dynamic append, breakpoint provenance와 guest-target 복원을
  연결했습니다. 실행 파일 이름·해시·주소 기반 분기는 추가하지 않았습니다.
- 로더는 이 기능을 `aot-dbt`에만 켜고 site 수를 기록합니다.

### 검증

- Release 빌드: `repiu_aot_probe`, `repiu_loader_win32` 성공
- 전체 AOT probe: exit 0, `dbt_direct_edge_dispatch_all=true`
- pumpit2 `aot-dbt`, 3초: site 1, cache build 성공, 요청 timeout까지 실행, process exit 0
- pumpit1 `aot-dbt`, 3초: site 0, 요청 timeout까지 실행, process exit 0
- pumpit2 `aot`: 기존 fail-closed 메시지 유지, process exit 1

### 결론

pumpit2에 특화된 예외 없이 정적 CFG의 과잉 fall-through를 AOT-DBT runtime 경계에서
일반적으로 처리했습니다. pumpit2 AOT-DBT 이미지 생성 차단은 해소됐습니다. 실제 플레이,
CD-DA 전환, 입력과 렌더링은 장시간 사용자 검증 범위로 남습니다.

## English

### Root cause

A direct call at `0x010FB9D1` invokes a return-address-consuming thunk at `0x010EFE34`.
Conservative reachability also followed `0x010FB9D6` into data, leaving one unresolved
`kBlockFallthrough` fixup at `0x010FB9E5 -> 0x010FB9E6`. The completed-image contract then
rejected every AOT-family backend before execution.

### Implementation

- Added a shared AOT-DBT build option and image-relative direct-edge site metadata.
- Routed only unresolved direct call, jump, conditional, and fall-through fixups through
  a 21-byte tail stub; mapped edges remain direct.
- Added a Win32 x86 host-stack thunk that validates the site, reuses
  `ResolveAotTransferTarget`, and fails closed through a site-owned INT3.
- Wired static placement, dynamic append, breakpoint provenance, and guest-target recovery.
  No executable-name, hash, or address special case was added.
- Enabled the option only for AOT-DBT and exposed the emitted site count in the loader log.

### Validation and conclusion

The Release probe and loader targets built successfully. The full probe exited zero with
`dbt_direct_edge_dispatch_all=true`. A three-second pumpit2 AOT-DBT smoke emitted one site
and ran to the requested timeout; pumpit1 emitted zero sites. Plain AOT retained its prior
fail-closed rejection. The pumpit2 AOT-DBT image-build blocker is resolved generically;
long-run gameplay, CD-DA transitions, input, and rendering remain for user validation.