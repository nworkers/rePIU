# 20260801-381 Music Select HLE Hotspot Attribution / Music Select HLE 핫스팟 귀속

## 한국어

### 목적

Music Select 실행 중 남아 있는 HLE 비용이 어떤 게스트 명령에서 발생하는지, 그리고 과거 세그먼트 읽기 fast-path 기록이 현재 소스와 일치하는지 확인합니다.

### 확인 범위

1. single-step 핫스팟 캡처의 상위 EIP를 원본 `PIU.EXE` 바이트와 대조합니다.
2. 현재 AOT 계획과 code-cache 방출기를 검토합니다.
3. Task 310의 설계·작업 로그 주장과 같은 커밋 직전의 소스를 비교합니다.

### 설계 판단

이번 작업은 동작을 변경하지 않습니다. 먼저 실제 경계 명령의 분포와 처리 비용을 확정한 뒤, 각 명령군별로 안전한 fast-path 여부를 따로 설계합니다. 세그먼트 selector shadow 상태를 무시하고 원본 `mov r32, Sreg`를 네이티브로 복사해서는 안 됩니다.

## English

### Purpose

Attribute the remaining Music Select HLE cost to guest instructions and verify whether the historical segment-read fast-path record agrees with the current source.

### Scope

1. Compare the top EIPs in the single-step hotspot capture with bytes in the original `PIU.EXE`.
2. Review the current AOT plan and code-cache emitter.
3. Compare Task 310's design and work-log claims with the source immediately before the commit that added those documents.

### Design decision

This task changes no runtime behavior. It first establishes the distribution and cost of actual boundary instructions; any safe fast-path must then be designed per instruction family. A native copy of `mov r32, Sreg` is not valid because it would ignore the guest shadow-selector state.
