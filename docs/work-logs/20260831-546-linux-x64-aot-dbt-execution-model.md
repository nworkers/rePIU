# 20260831-546 Linux x64 AOT/DBT 실행 모델 작업 로그

## 한국어

### 결과

Linux x64 경로를 기존 i386 native entry의 단순 재빌드로 취급하지 않고, guest
32비트 상태와 host 64비트 실행 상태를 분리하는 x86-64 AOT/DBT 모델을 정리했습니다.
특히 host RSP를 guest ESP로 바꾸지 않고, 이름 있는 x64 dispatch frame과 SysV ABI
bridge를 새 계약으로 두었습니다. `kCopy`, segment, stack/control, pointer relocation,
signal fault 복원도 각각 x64 전용 검증 대상이 됩니다.

이번 단위는 설계·작업 지시서만 작성했으며 실행 코드 변경은 없습니다. 다음 단위는
frame/type header와 synthetic ABI probe부터 시작해야 합니다.

### 검증

설계 문서와 작업 지시서의 범위가 기존 i386 경로를 변경하지 않는지 정적 검토했습니다.

## English

### Result

Defined Linux x64 as an x86-64 AOT/DBT host model that separates fixed-width 32-bit
guest state from 64-bit host execution state, rather than as a rebuild of the i386
native entry. Host RSP stays a host stack; a named x64 dispatch frame and a SysV ABI
bridge become the new contract. `kCopy`, segments, stack/control operations, pointer
relocations, and signal fault recovery are separate x64 validation targets.

This unit contains design and work-order documents only; it adds no execution code. The
next unit starts with the frame/type header and a synthetic ABI probe.

### Verification

Reviewed the design and work-order scope to ensure the existing i386 path is unchanged.
