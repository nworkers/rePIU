# Task 236 작업 로그 / Work Log

## 결과 / Result

`fxTMGetTMBlock()`의 비정상 크기 `0x030FEE17`은 텍스처 크기가 아니라 Glide import-resolver thunk 주소임을 확인했습니다. `_GRTEXTEXTUREMEMREQUIRED@8`을 구현하여 1×1 ARGB4444 관측 입력에 대해 8바이트 정렬 크기를 EAX로 반환하도록 했습니다. 기존 DOS `AX=4CFF` 종료와 `fxTMGetTMBlock()` 오류는 재현되지 않았습니다.

The abnormal `fxTMGetTMBlock()` size `0x030FEE17` was confirmed to be a Glide import-resolver thunk address, not a texture size. `_GRTEXTEXTUREMEMREQUIRED@8` now returns the aligned size for the observed 1×1 ARGB4444 input in EAX. The former DOS `AX=4CFF` exit and `fxTMGetTMBlock()` error no longer reproduce.

## 변경 / Changes

- `grTexMinAddress`/`grTexMaxAddress`의 실제 ABI를 남기는 고정 크기 진단 trace를 추가했습니다.
- `GrTexInfo` 기반 텍스처 메모리 크기 계산을 공용 Glide HLE에 추가했습니다.
- 관측된 texture upload/source/sampler/combine 호출은 현재 OpenGL 텍스처 저장소가 없는 렌더링 경계 no-op로 stdcall ABI만 보존합니다.

- Added a fixed-size diagnostic trace for the observed `grTexMinAddress`/`grTexMaxAddress` ABI.
- Added shared Glide HLE texture-memory calculation from `GrTexInfo`.
- Observed texture upload/source/sampler/combine calls currently preserve only their stdcall ABI as rendering-boundary no-ops because OpenGL texture storage is not implemented yet.

## 검증 / Verification

- `cmd /c scripts\\build_win32_x86.bat`: 성공.
- `REPIU_EXECUTION_BACKEND=aot-dynamic`, `REPIU_EXECUTION_TIMEOUT_MS=0`, supervisor 180000 ms: 기존 오류를 통과하고 약 90초까지 진행. 다음 frontier는 `_GRHINTS@8`입니다.

- `cmd /c scripts\\build_win32_x86.bat`: passed.
- With `REPIU_EXECUTION_BACKEND=aot-dynamic` and `REPIU_EXECUTION_TIMEOUT_MS=0`, a 180000 ms supervisor request passed the former error and ran to about 90 seconds. The next frontier is `_GRHINTS@8`.
