# prefix segment store 작업 로그

`66 8C /r`를 software segment-store HLE로 처리했습니다. PIU saved client GS가 Win32 `002Bh`에서 DOS/4GW `0020h`으로 교정됐으며 Win32 x86 빌드와 장시간 실행을 통과했습니다.

# Prefixed Segment Store Work Log

Handled `66 8C /r` through software segment state, correcting PIU's saved GS from `002Bh` to `0020h`.
