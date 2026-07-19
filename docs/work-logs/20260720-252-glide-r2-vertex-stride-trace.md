# Glide R2 정점 stride trace 작업 로그

## 결과

16개 `grDrawTriangle` trace에서 60바이트 정점 producer stride를 확인했다. 포인터는 `C640/C67C/C6B8`의 0x3C 간격을 반복했고, 세 번째 인자는 `C6F4`처럼 0x3C의 배수 간격으로 선택됐다. 각 entry는 세 포인터의 15 dword를 ring trace에 보관한다.

## 검증

* `scripts\\build_win32_x86.bat`: 성공.
* 직접 loader `pumpit1`, aot-dynamic, 내부 timeout disabled, 85초: 16개 triangle 수집.
* x는 288부터 8 단위로 증가, y는 329.9375로 유지되어 첫 두 dword가 화면 좌표 float라는 해석을 강화한다.

## 다음 단계

15 dword의 색상·depth·TMU 필드를 추가 표본으로 분류한 뒤 compact vertex 변환을 설계한다.

# Glide R2 Vertex Stride Trace Work Log

## Result

Sixteen `grDrawTriangle` traces confirm a 60-byte producer stride. Pointers repeatedly use `C640/C67C/C6B8` at 0x3C intervals, and the third argument selects a multiple-of-0x3C entry such as `C6F4`. Each entry retains 15 dwords for its three pointers in a ring trace.

## Verification

* `scripts\\build_win32_x86.bat`: succeeded.
* Direct loader `pumpit1`, aot-dynamic, internal timeout disabled, 85 seconds: collected 16 triangles.
* x advances from 288 in 8-unit steps while y remains 329.9375, strengthening the interpretation of the first two dwords as screen-coordinate floats.

## Next step

Classify color, depth, and TMU fields among the 15 dwords from additional samples before designing compact-vertex conversion.
