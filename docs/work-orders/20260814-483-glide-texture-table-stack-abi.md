# Glide texture-table guest stack ABI 정정 작업 지시

설계: [20260814-483-glide-texture-table-stack-abi.md](../design/20260814-483-glide-texture-table-stack-abi.md)

1. Win32 Glide boundary에 `_GRTEXDOWNLOADTABLE@12` frame decoder를 추가합니다.
2. handler가 네 dword를 읽고 `TMU/type/data`를 올바른 위치에서 사용하도록 수정합니다.
3. handler의 성공 복귀 stack advance를 16바이트로 정정합니다.
4. 정상 palette frame, 짧은 frame과 null 출력에 대한 AOT probe를 추가합니다.
5. 표준 palette 상위 바이트 무시와 P_8/AP_88 alpha 의미를 probe로 고정합니다.
6. Win32 x86 Debug/Release에서 probe와 애플리케이션을 빌드하고 probe를 실행합니다.
7. 새로 확인된 ABI·palette 결론을 Glide 분석/KB와 작업 로그에 반영합니다.
8. P_8/AP_88 원본 texel을 보존하고 palette download 뒤 기존 indexed texture를
   재디코드·재업로드합니다.
9. 실기에서 확인된 palette refresh 성능 저하를 후속 최적화 항목으로만 문서화합니다.

## 완료 조건

합성 frame에서 `TMU=0`, `type=2`, `data`가 정확히 복원되고 cleanup이 16바이트여야
합니다. 기존 probe가 모두 통과하고 실제 재실행에서 동일 호출의 unsupported issue와
Glide ABI reject가 발생하지 않아야 합니다.

---

# Glide Texture-Table Guest Stack ABI Correction Work Order

Design: [20260814-483-glide-texture-table-stack-abi.md](../design/20260814-483-glide-texture-table-stack-abi.md)

1. Add a `_GRTEXDOWNLOADTABLE@12` frame decoder to the Win32 Glide boundary.
2. Make the handler read four dwords and use the correct TMU/type/data slots.
3. Correct successful stack advancement to 16 bytes.
4. Add AOT probes for a valid palette frame, a short frame, and null output.
5. Probe the ignored standard-palette high byte and P_8/AP_88 alpha semantics.
6. Build and run the Win32 x86 Debug/Release probe and application.
7. Record the confirmed ABI and palette findings in Glide analysis/KB and the
   work log.
8. Retain P_8/AP_88 texels and re-decode/re-upload existing indexed textures
   after a palette download.
9. Document the runtime-confirmed palette-refresh performance loss as deferred
   optimization only.

## Completion criteria

The synthetic frame must decode `TMU=0`, `type=2`, and data exactly with a
16-byte cleanup. Existing probes must pass, and a real rerun must show neither
the same unsupported issue nor a Glide ABI rejection.
