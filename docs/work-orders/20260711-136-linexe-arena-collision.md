# LINEXE arena 충돌 교정 작업 지시

HLE 세 페이지를 arena 상단으로 이동하고 동적 범위 끝을 명시합니다. 실제 allocator OR를 복원한 뒤 빌드와 반복 실행에서 중첩 예외가 사라지는지 검증합니다.

# LINEXE Arena Collision Work Order

Move the three HLE pages to the arena top, record the dynamic range end, restore real allocator OR, then build and repeat runtime validation.
