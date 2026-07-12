# MSCDEX INT 2Fh probe 작업 로그 / Work Log

## 한국어

`INT 2Fh AX=1500h` 직접 호출과 `INT 31h AX=0300h, BL=2Fh`로 전달된 real-mode frame의 `AX=1510h`을 확인했습니다. CD-ROM drive가 없는 최소 환경으로 `BX=CX=0`, 그리고 `AX=000Fh`와 frame CF를 반환하도록 구현했습니다. 이 변경 후 guest는 초기 probe를 통과해 Glide 초기화와 DOS 종료 경로까지 진행했습니다.

## English

Observed direct `INT 2Fh AX=1500h` and an `AX=1510h` real-mode frame passed through `INT 31h AX=0300h, BL=2Fh`. The minimal no-CD-ROM environment now returns `BX=CX=0`, and `AX=000Fh` with frame CF for the device request. The guest then reaches Glide initialization and its DOS termination path.
