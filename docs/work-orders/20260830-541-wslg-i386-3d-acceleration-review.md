# 20260830-541 WSLg i386 3D 가속 경로 검토 작업 지시서

## 한국어

### 배경

WSLg는 3D 가속을 지원하지만, 기존 Linux 성능 측정에서 `llvmpipe`와
`Accelerated: no`가 관찰되었습니다. 이 결과가 WSLg 전체의 한계인지, 현재
32-bit i386 게임 실행 환경의 Mesa/D3D12 런타임 제약인지 구분합니다.

### 작업 항목

1. 같은 WSLg 세션에서 기본 renderer와 D3D12 renderer를 비교합니다.
2. WSLg vGPU 장치와 64-bit/32-bit Mesa D3D12 DRI 모듈을 확인합니다.
3. Linux 게임 실행 파일의 ABI와 GL 라이브러리 경로를 확인합니다.
4. paletted texture 성능 분석에서 환경 요인과 Linux 포트 기능 누락을 분리합니다.
5. 확인됨·추정·미확정 사항과 다음 검증 절차를 기록합니다.

### 범위 및 안전성

- 소스 코드와 실행 파일은 변경하지 않습니다.
- 패키지 설치나 시스템 설정 변경은 수행하지 않습니다.
- WSLg 및 Mesa 상태는 읽기 전용 명령으로만 확인합니다.

### 판정 기준

- `Vendor: Microsoft Corporation`, `Device: D3D12 (...)`, `Accelerated: yes`이면
  해당 GL client가 WSLg D3D12 경로를 사용합니다.
- 게임이 i386이면 i386 GL loader가 로드할 D3D12 DRI 모듈의 존재 여부를 별도로
  확인해야 합니다. 64-bit `glxinfo` 결과만으로 게임 프로세스의 renderer를
  단정하지 않습니다.

## English

### Background

WSLg supports 3D acceleration, but the previous Linux performance measurement showed
`llvmpipe` and `Accelerated: no`. This task distinguishes a limitation of WSLg itself
from a Mesa/D3D12 runtime limitation affecting the 32-bit i386 game process.

### Work items

1. Compare the default and explicit D3D12 renderers in the same WSLg session.
2. Check the WSLg vGPU device and the 64-bit/32-bit Mesa D3D12 DRI modules.
3. Check the Linux executable ABI and its GL library paths.
4. Separate runtime-environment effects from missing Linux paletted-texture code.
5. Record confirmed, inferred, unresolved findings, and the next verification step.

### Scope and safety

- Do not change source code or executables.
- Do not install packages or change system settings.
- Inspect WSLg and Mesa state with read-only commands only.

### Decision criteria

- `Vendor: Microsoft Corporation`, `Device: D3D12 (...)`, and `Accelerated: yes`
  indicate that the queried GL client uses the WSLg D3D12 path.
- For an i386 game, separately verify that the i386 GL loader can load a D3D12 DRI
  module. A 64-bit `glxinfo` result must not be treated as proof of the game
  process's renderer.
