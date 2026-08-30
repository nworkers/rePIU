# 20260830-540 Linux paletted texture 경로 검토 작업지시서

## 한국어

### 배경

Win32에서 성능이 낮았던 구간이 paletted texture 사용 구간이라는 사용자 관측을
반영하여, Linux 포트가 같은 기능을 빠뜨렸는지와 Linux OpenGL 환경에서 추가로
불리한 점이 있는지 확인한다.

### 작업 항목

1. Linux 빌드가 사용하는 팔레트·텍스처 변환 경로를 Win32 경로와 대조한다.
2. Linux 실행 로그에서 P_8/AP_88 사용량, palette 변경·refresh 횟수, decode/upload
   비용과 실패 여부를 확인한다.
3. Linux OpenGL renderer가 하드웨어 가속인지 확인한다.
4. 포트 누락, 공용 구현의 비용, Linux 실행 환경의 비용을 구분한다.
5. 확인된 사실·추정·미확정 사항을 누적 분석 문서와 작업 로그에 남긴다.

### 범위

- 소스 코드: 변경하지 않는다.
- 실행 파일·게스트 코드: 변경하지 않는다.
- 계측: 기존 Linux i386 Release 실행 파일과 기존 환경 변수를 사용한다.
- 이번 작업에서는 paletted texture 최적화를 구현하지 않는다.

### 판정 기준

- Linux가 별도 누락 경로 없이 P_8/AP_88을 처리하는지 확인한다.
- palette refresh 실패·palette 누락·OpenGL 오류를 확인한다.
- refresh 호출 비용과 소프트웨어 rasterizer의 texture sampling 비용을 분리해
  전체 5 fps 하락의 설명 가능 범위를 구분한다.

## English

### Background

The user observed that a slow Win32 section was the section using paletted textures.
This task checks whether the Linux port omitted any part of that path and whether the
Linux OpenGL environment adds a disadvantage.

### Work items

1. Compare the palette and texture conversion path used by the Linux build with the
   Win32 path.
2. Inspect the Linux run for P_8/AP_88 usage, palette changes and refreshes, decode
   and upload cost, and failures.
3. Identify whether the Linux OpenGL renderer is hardware accelerated.
4. Separate a porting omission, cost in the shared implementation, and cost caused by
   the Linux runtime environment.
5. Record confirmed, inferred, and unresolved findings in the cumulative analysis and
   work log.

### Scope

- Source code: unchanged.
- Executable and guest code: unchanged.
- Measurement: existing Linux i386 Release binary and existing environment variables.
- No paletted-texture optimization is implemented in this task.

### Decision criteria

- Confirm whether Linux handles P_8/AP_88 without a missing platform path.
- Check refresh failures, missing palettes, and OpenGL errors.
- Distinguish refresh-call cost from software-rasterizer texture-sampling cost and
  determine how much of the sustained 5 FPS drop they can explain.
