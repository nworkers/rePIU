# rePIU 기술 지식 기반 색인

이 디렉터리는 프로젝트를 이해하는 데 필요한 일반 기술 지식을 정리합니다. PIU
바이너리에서 직접 확인한 사실은 `docs/analysis/`에 기록합니다. 외부 자료에서 얻은
정의와 규약에는 원문 링크를 함께 둡니다.

```mermaid
flowchart TD
    DOS["DOS/4GW"] --> DPMI["DPMI services"]
    DOS --> LOWMEM["DOS/4GW Low-Memory Model"]
    DPMI --> INT["Important interrupts"]
    CPU["x86 segmentation and width"] --> HLE["Exception-driven HLE"]
    LE["LE format and relocation"] --> HLE
    LOWMEM --> HLE
    TERMS["Arena / sentinel / shadow"] --> HLE
    SMC["Self-modifying code"] --> CACHE["AOT code-cache coherency"]
    CACHE --> DYNAREC["dynarec vs exception-driven AOT"]
    DYNAREC --> HLE
    CACHE --> HLE
    CHD["CHD / ISO9660"] --> MSCDEX["MSCDEX / CD-DA"]
    YMZ["YMZ280B PCM/ADPCM"] --> SOUND["PIU10 board sound"]
    CAT["CAT702 serial security"] --> PIU10["PIU10 flash/MP3 board"]
```

## 문서

* [DOS/4GW와 DPMI](dos4gw-and-dpmi.md)
* [DOS/4GW 저지대 메모리 모델 및 관용](dos4gw-low-memory-model.md)
* [x86 segmentation과 16/32비트 처리](x86-segmentation-and-bit-width.md)
* [Arena, sentinel, shadow memory 용어](memory-terms.md)
* [주요 DOS/DPMI interrupt](important-interrupts.md)
* [LE 실행 형식과 fixup/relocation](le-format-and-relocation.md)
* [HLE와 예외 기반 직접 실행](hle-and-exception-driven-execution.md)
* [동적 재컴파일(dynarec)과 예외 기반 AOT 디스패치](dynamic-recompilation-and-aot-dispatch.md)
* [Self-modifying code와 code-cache 일관성](self-modifying-code-and-cache-coherency.md)
* [CHD와 ISO9660](chd-and-iso9660.md)
* [MSCDEX와 CD-DA](mscdex-and-cd-da.md)
* [Glide 텍스처 LOD, aspect ratio, 포맷](glide-texture-lod-and-formats.md)
* [Glide primitive와 면 culling](glide-primitives-and-culling.md)
* [YMZ280B PCM/ADPCM 디코더](ymz280b-pcm-adpcm-decoder.md)
* [CAT702 PIU 직렬 보안 장치](cat702-piu-security-device.md)

# rePIU Technical Knowledge Base Index

This directory documents generally applicable technical background needed to
understand the project. Facts confirmed specifically from the PIU binary belong
in `docs/analysis/`. Definitions and contracts derived from external material
include source links.

* [DOS/4GW and DPMI](dos4gw-and-dpmi.md)
* [DOS/4GW low-memory model and tolerance](dos4gw-low-memory-model.md)
* [x86 segmentation and 16/32-bit handling](x86-segmentation-and-bit-width.md)
* [Arena, sentinel, and shadow-memory terminology](memory-terms.md)
* [Important DOS/DPMI interrupts](important-interrupts.md)
* [LE executable format and fixup/relocation](le-format-and-relocation.md)
* [HLE and exception-driven direct execution](hle-and-exception-driven-execution.md)
* [Dynamic recompilation (dynarec) vs exception-driven AOT dispatch](dynamic-recompilation-and-aot-dispatch.md)
* [Self-modifying code and code-cache coherency](self-modifying-code-and-cache-coherency.md)
* [CHD and ISO9660](chd-and-iso9660.md)
* [MSCDEX and CD-DA](mscdex-and-cd-da.md)
* [Glide texture LOD, aspect ratio, and formats](glide-texture-lod-and-formats.md)
* [Glide primitives and face culling](glide-primitives-and-culling.md)
* [YMZ280B PCM/ADPCM decoder](ymz280b-pcm-adpcm-decoder.md)
* [CAT702 PIU serial security device](cat702-piu-security-device.md)
