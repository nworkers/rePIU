# Glide R2 정점 stride trace 작업 지시

1. 16-entry triangle 관측 ring trace를 추가한다.
2. triangle마다 세 정점의 60바이트를 안전하게 기록한다.
3. 실시간 진단으로 pointer와 좌표 후보를 출력한다.
4. Win32 x86 debug 빌드와 직접 loader 관찰로 확인한다.

# Glide R2 Vertex Stride Trace Work Order

1. Add a 16-entry triangle observation ring trace.
2. Safely record 60 bytes for all three vertices per triangle.
3. Print pointers and coordinate candidates in live diagnostics.
4. Verify by Win32 x86 debug build and direct-loader observation.
