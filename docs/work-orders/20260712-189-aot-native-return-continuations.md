# AOT native return continuation 작업 지시

1. direct call의 push immediate를 cache-return fixup으로 기록합니다.
2. Win32 placement/dynamic append에서 guest fallthrough를 cache 주소로 patch합니다.
3. `C3/C2`를 native cache bytes로 발행합니다.
4. indirect call dispatcher가 cache fallthrough를 push하도록 갱신합니다.
5. 제한 시간 PIU 관찰에서 return dispatcher 감소와 resource progress를 비교합니다.

# AOT Native Return Continuation Work Order

1. Record direct-call push immediates as cache-return fixups.
2. Patch guest fallthroughs to cache addresses during Win32 placement/dynamic append.
3. Emit `C3/C2` as native cache bytes.
4. Make the indirect-call dispatcher push cache fallthroughs.
5. Compare return-dispatch reduction and resource progress in bounded PIU runs.
