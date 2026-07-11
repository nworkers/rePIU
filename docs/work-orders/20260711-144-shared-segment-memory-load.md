# 공용 segment memory-load 작업 지시

1. prefix scan과 segment register 선택을 구현한다.
2. `8A/8B` byte·word·dword를 공용 주소 decoder와 selector read에 연결한다.
3. single-step/일반 dispatch에서 특수 handler보다 먼저 호출한다.
4. root selector, module name, export slot 해석을 빌드·실행 검증한다.

# Shared Segment Memory-Load Work Order

Implement shared prefix, width, address, and selector handling; dispatch before special cases; then verify root, module name, and export resolution.
