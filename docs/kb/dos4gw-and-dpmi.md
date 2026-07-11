# DOS/4GW와 DPMI

## DOS/4GW

DOS/4GW는 Watcom C/C++로 만든 32-bit protected-mode DOS 프로그램에 포함되던 DOS extender다. 프로그램은 32-bit x86 코드를 실행하지만 파일, console, process 같은 DOS 기능은 real-mode DOS와 extender가 제공하는 변환 계층을 통해 사용한다. Open Watcom 문서와 도구는 [Open Watcom 공식 사이트](https://openwatcom.org/)와 [공식 GitHub 조직](https://github.com/open-watcom)에서 확인할 수 있다.

## DPMI

DPMI(DOS Protected Mode Interface)는 protected-mode DOS 프로그램이 descriptor, DOS memory, real-mode interrupt simulation, exception/interrupt vector, version 정보 등을 요청하는 표준 interface다. 함수 번호와 register contract는 [DJGPP가 제공하는 DPMI 1.0 사양 HTML](https://www.delorie.com/djgpp/doc/dpmi/)에서 확인할 수 있다.

DPMI host와 DOS extender는 같은 개념이 아니다. extender는 executable을 준비하고 runtime을 제공하며, DPMI host는 protected-mode service contract를 제공한다. 한 제품이 두 역할의 일부를 함께 수행할 수 있다.

## rePIU에서의 적용

rePIU는 DOS/4GW 전체를 복제하거나 CPU 전체를 emulation하지 않는다. 원본 32-bit 코드를 직접 실행하고 관찰된 DOS/DPMI service만 HLE로 제공한다. 따라서 지원하지 않는 DPMI 함수는 성공을 위장하지 않고 다음 분석 대상으로 남겨야 한다.

# DOS/4GW and DPMI

DOS/4GW is a DOS extender used by 32-bit protected-mode programs built with Watcom tools. DPMI is the standardized service interface for descriptors, DOS memory, simulated real-mode interrupts, vectors, and host capabilities. See the [Open Watcom project](https://openwatcom.org/) and the [DPMI 1.0 specification mirror maintained by DJGPP](https://www.delorie.com/djgpp/doc/dpmi/).

rePIU directly executes original 32-bit code and HLEs only observed DOS/DPMI boundaries. It does not pretend that unimplemented services succeeded.
