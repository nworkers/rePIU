# 20260801-384 Segment Read 기본 승격 작업 로그 / Work Log

설계: [20260801-384-segment-read-default-promotion.md](../design/20260801-384-segment-read-default-promotion.md)

작업 지시: [20260801-384-segment-read-default-promotion.md](../work-orders/20260801-384-segment-read-default-promotion.md)

## 한국어

### 측정 결론

- 기준/활성 capture의 고정 경로 지표 `0x030F536C=5,471`, DOS `AH=3Bh=580`이 일치했습니다.
- guarded read는 `0x030F536A` boundary를 제거했습니다.
- frame 정규화 결과 cycles/frame -30.14%, exceptions/frame -29.70%, HLE outcomes/frame -51.52%, segment-store HLE/frame -94.26%였습니다.
- 전체 평균은 21.64 FPS에서 31.00 FPS로 증가했지만 capture 길이가 달라 이 수치는 보조 증거로만 사용했습니다.

### 구현 및 검증

- `aot-dbt`에서 환경 변수 미지정 시 guarded segment-read를 기본 활성화했습니다.
- `REPIU_AOT_GUARDED_SEGMENT_READ=0|off|false`와 알 수 없는 값은 opt-out이며 다른 backend는 비활성화됩니다.
- Release Win32 loader 빌드가 성공했습니다. 기존 C4819 경고만 남았습니다.
- 환경 변수 미지정 1초 실행은 `enabled/sites: true/54`, 명시적 `0` 실행은 `false/0`을 기록했고 둘 다 정상 timeout했습니다.
- 전체 AOT probe는 종료 코드 0이며 모든 guarded segment-read 항목과 `selector_guard_all=true`를 확인했습니다.

## English

### Measurement conclusion

- Fixed-path markers matched between baseline and enabled captures: `0x030F536C=5,471` and DOS `AH=3Bh=580`.
- Guarded read removed the `0x030F536A` boundary.
- Frame-normalized results were cycles/frame -30.14%, exceptions/frame -29.70%, HLE outcomes/frame -51.52%, and segment-store HLE/frame -94.26%.
- Raw average throughput rose from 21.64 to 31.00 FPS, but capture-duration differences make it supporting evidence only.

### Implementation and verification

- Guarded segment-read is now enabled by default for `aot-dbt` when the environment variable is unset.
- `REPIU_AOT_GUARDED_SEGMENT_READ=0|off|false` and unknown values opt out; other backends remain disabled.
- The Release Win32 loader build passed with only pre-existing C4819 warnings.
- A one-second unset-variable run logged `enabled/sites: true/54`; an explicit `0` run logged `false/0`. Both reached normal timeout.
- The full AOT probe exited zero with every guarded segment-read check and `selector_guard_all=true`.
