# Task 755: WSL의 음악 속도 조사 작업 지시

설계: [20260928-755](../design/20260928-755-wsl-audio-rate-and-clock-tug-of-war.md)

## 한국어

1. pumpitea를 tick 시계 둘에서 75초씩 돌려 MP3 디코드 속도와 시계 어긋남 보고를 비교한다.
2. 사운드 서버의 monitor를 녹음해 출력 프레임 수를 두 시계로 센다(게임 없이).
3. Windows 시계와 NTP의 차이, WSL의 시간 동기화 상태를 읽는다.
4. 결과와 사용자가 해 볼 수 있는 조치를 작업 로그·analysis·README에 적고 커밋한다. 코드는 바꾸지 않는다.

## English

1. Run pumpitea on both tick clocks for 75 s each and compare the MP3 decode rate and the report of the
   clocks' divergence.
2. Record the sound server's monitor and count its output frames against both clocks (no game).
3. Read the difference between the Windows clock and NTP, and WSL's time synchronisation state.
4. Put the results and what the user can try in the work log, the analysis and README, and commit. No
   code changes.
