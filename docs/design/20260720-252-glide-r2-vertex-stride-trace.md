# Glide R2 정점 stride trace 설계

## 확인된 사실

첫 `grDrawTriangle`의 포인터는 `0x0383C640`, `0x0383C67C`, `0x0383C6F4`였으며 첫 두 포인터 차이는 60바이트다. 기존 72바이트 가정은 다음 정점 자료를 섞어 캡처한다.

## 설계

최대 16개 triangle을 ring trace로 기록한다. 각 entry는 세 포인터, 각 포인터의 60바이트(15 dword), readable flag를 보관한다. 실시간 로그는 entry별 포인터와 첫 두 float dword를 출력한다. 렌더링과 ABI는 변경하지 않는다.

## 판정 기준

여러 entry에서 포인터 간격이 60바이트와 그 배수로 유지되고 첫 두 dword가 화면 좌표 범위의 float로 일관되면, PIU producer layout의 최소 stride를 60바이트로 확정한다. 그 뒤 색상/깊이/TMU 필드의 의미를 별도 표본으로 분석한다.

# Glide R2 Vertex Stride Trace Design

## Confirmed fact

The first `grDrawTriangle` used pointers `0x0383C640`, `0x0383C67C`, and `0x0383C6F4`; the first pair differs by 60 bytes. The old 72-byte assumption captures adjacent entry data.

## Design

Record up to 16 triangles in a ring trace. Each entry holds three pointers, 60 bytes (15 dwords) per pointer, and readability flags. Live output reports pointers and the first two float dwords. Rendering and ABI remain unchanged.

## Decision rule

If multiple entries preserve 60-byte or multiple-of-60 pointer gaps and their first two dwords consistently decode as screen-coordinate floats, confirm a 60-byte minimum PIU producer stride before separately interpreting color, depth, and TMU fields.
