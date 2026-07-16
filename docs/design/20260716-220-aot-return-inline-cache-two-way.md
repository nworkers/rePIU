# AOT 반환 인라인 캐시 2엔트리 확장 설계
# Design: Two-Way AOT Return Inline Cache

## 1. 배경 (Background)

Task 219는 `pumpit1`의 "동결" 구간이 실제로는 비트스트림 디코드 루프이며, 헬퍼 함수
`0x030EE170`의 RET(`0x030EE1DA`)가 두 호출부(`0x030EE292`/`0x030EE300`)로 교대 반환하면서 AOT
반환 thunk의 **단일 엔트리** 인라인 캐시가 매 반환마다 miss → `int3` → VEH 왕복 → 재패치를
반복해 루프가 초당 약 820회로 약 1000배 감속됨을 확정했다. Task 204가 처리량 후보로 기록한
"indirect inline-cache 다중화"의 실증 사례다.

## 2. 현재 구조 (Current Structure)

`EmitReturnInlineCacheSlot`(`src/runtime/aot_code_cache.cpp:245`)이 방출하는 단일 엔트리 반환
thunk (RET `C3` 기준 27바이트):

```asm
    pushfd
    cmp dword [esp+4], imm32     ; 예측 반환 주소 (target_immediate_offset)
guard:                            ; 초기 E9 rel32(miss)+90, 패치 후 0F 85 rel32(miss)
    popfd
    lea esp, [esp+4]              ; 반환 주소 pop
    jmp rel32                     ; 캐시된 대상 (jump_displacement_offset)
miss:
    popfd
    int3                          ; dispatcher 진입 (miss_cache_offset)
```

`PatchWin32AotIndirectInlineCache`(`src/platform/win32/aot_code_cache_win32.cpp:387`)는 miss 시
imm32/점프 displacement를 **무조건 덮어쓰고** guard를 JNE(miss)로 설정한다 — 예측 슬롯이 1개라
반환 대상이 2개면 영구 스래싱한다.

## 3. 설계 (Design)

### 3.1 2엔트리 반환 thunk 레이아웃

반환 thunk(`is_return == true`)에 한해 비교/히트 블록을 하나 더 직렬 연결한다
(간접 call/jmp 사이트는 이번 범위에서 제외 — 기존 단일 엔트리 유지):

```asm
    pushfd
    cmp dword [esp+4], imm32_0    ; entry0 (target_immediate_offset)
guard0:                            ; 초기 E9(miss)+90, entry0 채워지면 0F 85 → entry1_compare
    popfd
    lea esp, [esp+pop]
    jmp rel32_0                    ; (jump_displacement_offset)
entry1_compare:                    ; (second_compare_offset)
    cmp dword [esp+4], imm32_1    ; entry1 (target_immediate_offset2)
guard1:                            ; 초기 E9(miss)+90, entry1 채워지면 0F 85 → miss
    popfd
    lea esp, [esp+pop]
    jmp rel32_1                    ; (jump_displacement_offset2)
miss:
    popfd
    int3                           ; (miss_cache_offset — 기존과 동일 의미)
```

EFLAGS는 thunk 진입 시의 `pushfd` 1회 사본이 스택에 유지되고 각 히트/미스 경로가 `popfd`로
복원하므로 두 번째 비교가 추가되어도 의미가 보존된다.

### 3.2 메타데이터 확장

`AotIndirectInlineCacheSite`(`include/repiu/runtime/aot_code_cache.h:40`)에 다음을 추가한다:
`bool has_second_entry`, `second_compare_offset`, `target_immediate_offset2`, `guard_offset2`,
`jump_displacement_offset2`. `AppendWin32AotDynamicImage`의 오프셋 재배치 루프는
`has_second_entry`일 때만 새 오프셋들에 `append_offset`을 더한다(0-센티널 오염 방지).
`IsAotInlineCacheMiss`/`FindAotGuestAddress`는 `miss_cache_offset` 의미가 그대로라 무변경.

### 3.3 패치 정책 (stateless)

`PatchWin32AotIndirectInlineCache`에서 `has_second_entry`인 사이트는 캐시 바이트의 현재 상태를
읽어 결정한다(별도 상태 저장 없음):

1. entry0이 채워져 있고(`bytes[guard0] == 0x0F`) `imm32_0 == guest_target`이면 entry0 갱신
   (세대 교체 후 재해석 케이스).
2. entry0이 비어 있으면(`bytes[guard0] == 0xE9`) entry0 채움. guard0의 JNE 대상은
   `second_compare_offset`.
3. entry1이 채워져 있고 `imm32_1 == guest_target`이면 entry1 갱신.
4. 그 외에는 entry1을 채움/교체. guard1의 JNE 대상은 `miss_cache_offset`.

3개 이상의 반환 대상이 교대하면 entry1이 교체 슬롯이 되어 스래싱이 남지만, 관측된 병목
(대상 2개)은 해소되고 정확성은 유지된다.

## 4. 영향 범위와 검증 (Impact & Verification)

* 게스트 가시 동작 불변(스택/EFLAGS 의미 보존). thunk 크기 27 → 약 46바이트.
* 검증: (1) Debug 재빌드, (2) `aot-dynamic pumpit1` 40초 구동에서 디코드 구간의
  `aot_boundary_guest_eip=0x030EE1DA` 고정과 `ret_dispatch` 폭주가 사라지고 dispatch/progress가
  재개되는지 확인, (3) 기본 trap 백엔드 30초 회귀 확인.
