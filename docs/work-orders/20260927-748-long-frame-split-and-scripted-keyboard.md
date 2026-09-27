# Task 748: 긴 프레임 분해 진단과 스크립트 키보드 작업 지시

설계: [20260927-748](../design/20260927-748-long-frame-split-and-scripted-keyboard.md)

## 한국어

1. `REPIU_INPUT_SCRIPT` 스크립트 키보드(`SdlInputScriptPlayer`)를 Glide 창 열림/닫힘에 붙이고,
   pumpitea 플레이 스크립트를 `scripts/input_scripts/`에 둔다.
2. `grBufferSwap`의 present를 감싸 `REPIU_GLIDE_LONG_FRAME_LOG` 긴 프레임 로그(sleep/guest/present 분해)를
   찍고, 페이서 지터 카운터를 최종 보고에 더한다.
3. `REPIU_DOS_ASSET_TRACE=all`로 자산 트레이스 상한을 푼다.
4. 페이싱 플레이를 스크립트로 반복해 곡 구간의 긴 프레임을 census·자산 트레이스·라이브 프로파일과
   맞추고, 비페이싱 로그·core probe(Linux·Win32)를 확인한 뒤 README·analysis·작업 로그를 갱신하고 커밋한다.

## English

1. Attach the `REPIU_INPUT_SCRIPT` scripted keyboard (`SdlInputScriptPlayer`) to the Glide window's
   open and close, with the pumpitea play script under `scripts/input_scripts/`.
2. Wrap the present in `grBufferSwap` for the `REPIU_GLIDE_LONG_FRAME_LOG` line (sleep/guest/present
   split) and add the pacer's jitter counters to the final report.
3. Lift the asset trace caps with `REPIU_DOS_ASSET_TRACE=all`.
4. Repeat paced scripted plays, match the in-song long frames against the census, the asset trace and
   the live profile, check the unpaced log and the core probes (Linux, Win32), update README, the
   analysis and the work log, and commit.
