# PIU10 DAC audio backlog 감사 가이드 / PIU10 DAC Audio Backlog Audit Guide

근거 설계: [Task 463](../design/20260809-463-piu10-dac-audio-backlog-audit.md)

## 한국어

PowerShell에서 다음과 같이 DAC 감사를 켜고 `pumpito`를 실행합니다.

```powershell
$env:REPIU_PIU10_DAC_AUDIT='1'
.\build\win32_x86_debug\Debug\repiu.exe pumpito
```

음악이 예상보다 일찍 끊기면 정상 종료한 뒤 `repiu_log.txt`에서
`data=0x0101`인 마지막 관련 줄을 찾습니다. 다음 필드를 함께 확인합니다.

- `pcm-queued-ms`: SDL stream에 남은 입력 PCM의 현재 형식 기준 시간
- `device-buffer-ms`: 한 audio-device 공급 chunk의 시간 범위
- `compressed-ring`: 아직 worker가 꺼내지 않은 MP3 byte
- `decoder-pending`: worker가 꺼냈지만 아직 decode하지 않은 MP3 byte
- `compressed-inflight`: 두 저장 위치를 포함하여 decoder가 아직 소비하지 않은 전체 byte
- `received`, `decoded`, `frame-sync`: transaction 시점의 진행 상태

`pcm-queued-ms`가 0~20 ms이고 두 compressed 값도 거의 0이면 곡 끝 경계와 일치할
가능성이 큽니다. 50~100 ms 이상이면 startup/device latency를 검토하고, 150~250 ms에
가깝거나 compressed backlog도 남아 있으면 guest DAC 시각이 HLE 출력보다 앞선 강한
증거로 분류합니다. 실행 후 환경변수를 제거하려면 다음을 사용합니다.

```powershell
Remove-Item Env:REPIU_PIU10_DAC_AUDIT -ErrorAction SilentlyContinue
```

## English

Enable DAC audit in PowerShell and run `pumpito`:

```powershell
$env:REPIU_PIU10_DAC_AUDIT='1'
.\build\win32_x86_debug\Debug\repiu.exe pumpito
```

If music cuts early, exit normally and find the relevant final `data=0x0101` record in
`repiu_log.txt`. Inspect `pcm-queued-ms`, `device-buffer-ms`, `compressed-ring`,
`decoder-pending`, `compressed-inflight`, `received`, `decoded`, and `frame-sync` together. Roughly 0--20 ms of PCM with
negligible compressed backlog is consistent with an end boundary. Values above 50--100 ms warrant
checking startup/device latency; roughly 150--250 ms or remaining compressed backlog is strong
evidence that the guest DAC timeline leads HLE output. Remove the environment variable afterwards
with:

```powershell
Remove-Item Env:REPIU_PIU10_DAC_AUDIT -ErrorAction SilentlyContinue
```
