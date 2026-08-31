# 20260901-554 Linux x64 code cache 배치 작업 지시서

## 한국어

### 목적

x64 host가 code cache를 배치할 수 있게 합니다. 설계는
[20260901-554](../design/20260901-554-linux-x64-code-cache-placement.md)입니다.

### 작업

- `PlaceAotCodeCache`가 64비트 host에서 하위 4 GiB 후보 사다리를 타게 한다. 후보는 guest
  arena 상단과 engine image `0x40000000` 사이에 둔다.
- 32비트 host에서는 사다리를 타지 않는다. i386 경로는 오늘과 같아야 한다.
- 사다리가 전부 실패하면 기존 hint 없는 요청으로 내려가고, 기존 4 GiB 초과 거절이 그대로
  마지막 방어선이 된다.
- 배치 결과(요청한 후보, 실제 base, 몇 번째에 성공했는지)를 placement에 남겨 측정 가능하게
  한다.
- 모든 호스트에서 도는 probe를 추가한다. long-mode image를 만들어 배치하고, base가 하위
  4 GiB인지와 `FindAotGuestAddress` 왕복이 맞는지 확인한다.

### 검증

- **Linux x64**: 변경 전에는 배치가 실패하고, 변경 후에는 성공해야 한다. 이 대비가 이
  작업의 증거다.
- **Linux i386 · Win32 x86**: 기존 probe가 전부 그대로 통과. 배치 주소는 사다리를 타지
  않으므로 이전과 같은 방식으로 정해진다.

## English

### Objective

Let an x64 host place a code cache. The design is
[20260901-554](../design/20260901-554-linux-x64-code-cache-placement.md).

### Work items

- Make `PlaceAotCodeCache` walk a ladder of below-4-GiB candidates on a 64-bit host, with
  the candidates between the guest arena's top and the engine image at `0x40000000`.
- Do not walk the ladder on a 32-bit host. The i386 path must be what it is today.
- If every candidate fails, fall through to the existing unhinted request, leaving the
  existing above-4-GiB refusal as the last line of defence.
- Record the outcome on the placement -- which candidate was asked for, what base came
  back, and which attempt succeeded -- so it can be measured.
- Add a probe that runs on every host: build a long-mode image, place it, and check the
  base is below 4 GiB and that `FindAotGuestAddress` round-trips.

### Verification

- **Linux x64**: placement fails before the change and succeeds after. That contrast is
  this task's evidence.
- **Linux i386 and Win32 x86**: every existing probe still passes, and the placement
  address is chosen the same way it was, because the ladder does not run there.
