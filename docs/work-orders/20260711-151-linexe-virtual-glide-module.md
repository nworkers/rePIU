# LINEXE 가상 Glide module 작업 지시

1. 범용 원거리 전이 관찰기를 처리 가능한 handler로 분리합니다.
2. `glide2x.ovl` 이름과 `LOADMODULE` service를 함께 검증합니다.
3. opaque virtual handle과 관찰된 bridge/wrapper frame의 원자적 반환 복원을 구현합니다.
4. Win32 x86 빌드와 supervisor 실행으로 다음 LINEXE service/ABI를 관찰합니다.
5. 결과와 다음 의사결정 항목을 분석·작업 로그에 기록합니다.

# LINEXE Virtual Glide Module Work Order

Convert the generic far-transfer observer into a handler, jointly validate the `glide2x.ovl` name and `LOADMODULE` service, return an opaque handle while removing the 12-byte bridge frame and resuming at the shared epilogue, run the Win32 x86 build and supervisor, and document the next LINEXE ABI and decision point.
