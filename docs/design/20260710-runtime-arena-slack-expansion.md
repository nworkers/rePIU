# Runtime arena slack 확장 설계

## 배경

최근 shadow memory 요약에서 `piu_1st`의 post-`spr.res` 쓰기 범위가 `0x025E7000` 이후 `0x02670E57`까지 확장되는 것으로 확인되었다. 현재 relocated runtime arena는 `0x02000000` 기준 `0x005E7000` 크기로 끝 주소가 `0x025E7000`이다.

즉 shadow memory는 arena 끝 직후부터 약 560KB 이상 범위의 쓰기를 대신 받고 있다. 이는 단일 변수 보정이라기보다 DOS/4GW runtime이 resize 이후 사용할 수 있다고 판단한 heap/metadata 영역이 host arena에 아직 반영되지 않은 상태로 해석한다.

## 설계

이번 단계에서는 완전한 DOS memory manager를 만들지 않고, 관측 기반 arena slack을 늘린다.

* 기존 runtime arena expansion slack: `0x00010000`
* 새 runtime arena expansion slack: `0x00100000`

Win32 placement는 arena reserve 전체를 `MEM_RESERVE | MEM_COMMIT`으로 확보하므로, slack 확장은 `IsGuestRangeReadable/Writable`이 참으로 판단하는 실제 guest memory 범위를 넓힌다. 기존 shadow memory HLE는 안전망으로 유지하되, 확장된 arena 안의 store는 실제 memory write로 처리되어야 한다.

## 기대 결과

`piu_1st`의 shadow memory min/max가 크게 줄거나, 최소한 기존 `0x025E7000` 직후의 넓은 shadow write가 실제 arena write로 흡수되어야 한다.

## 범위 밖

* DOS memory block chain/MCB를 구현하지 않는다.
* resize selector별 실제 block ownership 모델은 만들지 않는다.
* shadow memory HLE를 제거하지 않는다.

# Runtime Arena Slack Expansion Design

## Background

Recent shadow memory summaries showed that post-`spr.res` `piu_1st` writes extend from `0x025E7000` to `0x02670E57`. The current relocated runtime arena starts at `0x02000000` and has size `0x005E7000`, ending exactly at `0x025E7000`.

So shadow memory is covering writes immediately after the arena end across more than about 560KB. This looks less like a single variable workaround and more like a heap/metadata area that the DOS/4GW runtime believes is available after resize, but the host arena has not yet reflected it.

## Design

This step does not implement a full DOS memory manager. Instead, it increases the observed arena slack.

* Previous runtime arena expansion slack: `0x00010000`
* New runtime arena expansion slack: `0x00100000`

Win32 placement reserves and commits the entire arena with `MEM_RESERVE | MEM_COMMIT`, so increasing slack expands the real guest memory range accepted by `IsGuestRangeReadable/Writable`. The existing shadow memory HLE remains as a safety net, but stores inside the expanded arena should become real memory writes.

## Expected Result

The `piu_1st` shadow memory min/max range should shrink significantly, or at least the broad shadow writes immediately after `0x025E7000` should be absorbed into real arena writes.

## Out Of Scope

* Do not implement DOS memory block chains/MCBs.
* Do not add a selector-specific real block ownership model.
* Do not remove shadow memory HLE.
