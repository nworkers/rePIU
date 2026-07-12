# Zydis verified region 작업 로그 / Work Log

## 한국어

Zydis v4.1.1 tag와 pinned Zycore submodule에서 공식 `amalgamate.py`로 source/header를 생성하고 두 MIT 라이선스와 함께 vendoring했습니다. CMake에 C 언어와 정적 Zydis target을 추가하고, decoder/CFG 정책을 `verified_region_analyzer.*`로 분리했습니다.

Zydis는 instruction boundary, category, attributes, relative target을 제공하며 rePIU는 민감 명령과 제어 흐름을 fail-closed로 거부합니다. 기존 자체 decoder 코드는 제거했습니다. 중간 예외가 발생한 함수는 현재 실행의 verified cache에서 영구 거부합니다.

Win32 x86 Debug 빌드가 성공했습니다. 60초 PIU 실행에서 fast-path entry/return/cancel은 `19,437/19,431/6`이었고 guest fatal은 없었습니다. 실행 표본은 기존 `+0xDE1xx` unpack 병목을 벗어나 여러 후속 구간으로 진행했습니다.

## English

Generated the amalgamated source/header with official `amalgamate.py` from Zydis v4.1.1 and its pinned Zycore submodule, then vendored both MIT licenses. Added C language and a static Zydis CMake target, and separated decoder/CFG policy into `verified_region_analyzer.*`.

Zydis supplies instruction boundaries, categories, attributes, and relative targets; rePIU rejects sensitive instructions and control flow fail-closed. Removed the in-house decoder. A function that raises an intermediate exception is permanently rejected from the current run's verified cache.

The Win32 x86 Debug build passes. A 60-second PIU run recorded `19,437/19,431/6` fast-path entries/returns/cancellations without guest fatal output and progressed beyond the former `+0xDE1xx` unpack bottleneck.
