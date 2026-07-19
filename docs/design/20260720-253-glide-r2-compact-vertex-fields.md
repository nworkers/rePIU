# Glide R2 compact vertex 필드 관측 설계

60바이트 stride는 확인됐지만 dword 2~14의 의미는 미확정이다. 강제 종료되는 직접 loader 관찰에서도 원시 자료를 보존하기 위해, 최초 16개 triangle의 각 정점 15 dword를 실시간 stderr에 출력한다. 이 작업은 ABI와 렌더링을 바꾸지 않는다.

확인 기준은 dword별 triangle 간 변화, 정점 간 보간, 그리고 이미 확정된 x/y float와의 상관관계다. float/packed-color/texture-coordinate 후보는 관측 자료로만 분류한다.

# Glide R2 Compact Vertex Field Observation Design

The 60-byte stride is confirmed but dwords 2-14 remain unresolved. To retain raw evidence when direct-loader observation is forcibly stopped, print all 15 dwords for each vertex of the first 16 triangles to live stderr. This does not change ABI or rendering.

Classify candidates only from cross-triangle variation, cross-vertex interpolation, and correlation with the confirmed x/y floats.
