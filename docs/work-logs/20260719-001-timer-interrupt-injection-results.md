# Timer Interrupt Injection Results

## 개요
타이머 인터럽트 주입 구현 후 실제 PIU.EXE를 실행하여 검증한 결과입니다.

## Overview
These are the results of verifying the PIU.EXE execution after implementing timer interrupt injection.

## 검증 결과 (Verification Results)
1. **인터럽트 훅 여부 확인**: PIU.EXE는 DPMI `INT 31h AX=0205h` 또는 DOS `INT 21h AH=25h`를 통해 타이머 인터럽트(Vector 0x08 또는 0x1C)를 **가로채지 않습니다 (No Hooking)**.
2. **BIOS Timer Tick 의존**: 게임의 타이머 루프는 인터럽트 핸들러가 아니라 BIOS Data Area의 Timer Tick(선형 주소 `0x46C`)을 직접 읽어서 시간을 측정합니다.
3. **게임 루프 진행 확인**: Host Poller 스레드(`live_telemetry_snapshot.cpp`)가 `0x46C`의 틱 값을 18.2Hz로 업데이트하는 로직이 정상 작동하면서, 무한 반복되던 하드웨어 대기 루프(`grBufferSwap`)를 성공적으로 돌파했습니다.
4. **Glide 초기화 진입**: 대기 루프를 돌파한 게임 로직은 이후 데이터 파일(`spr.res` 등)을 로드하고 정상적으로 Glide 초기화 관문인 `grSstWinOpen` (Ordinal 76/72) 및 텍스처 다운로드 `grTexDownloadMipMap` (Ordinal 94)까지 도달했습니다.

## 결론 (Conclusion)
타이머 인터럽트 주입 코드는 정상적으로 통합 및 빌드되었으나 PIU.EXE가 이를 직접 사용하지는 않습니다. 대신 BIOS Timer Tick `0x46C` 업데이트를 통해 게임의 무한 루프 블로킹이 성공적으로 해결되었으며, 이제 렌더링 파이프라인(`grDrawTriangle` 등) 구현을 진행할 준비가 완료되었습니다.
