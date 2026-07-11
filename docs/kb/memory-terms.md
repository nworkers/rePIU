# Arena, sentinel, shadow memory 용어

## Arena

arena는 여러 allocation을 하나의 큰 memory region에서 관리하는 방식 또는 그 region을 뜻한다. 이 프로젝트의 runtime arena는 allocator 알고리즘 이름이라기보다 relocated image, guest stack, 관찰된 heap 확장을 담는 host-owned 연속 주소 범위를 가리킨다.

## Sentinel

sentinel은 목록의 끝, 빈 상태, 실패, 경계처럼 특별한 상태를 별도 flag 없이 표현하는 예약 값이나 객체다. 예를 들어 null pointer, `-1`, dummy head/tail node가 sentinel이 될 수 있다. sentinel의 의미는 자료구조마다 다르므로 값만 보고 의미를 단정하면 안 된다.

rePIU에서 관찰된 `0xFFFFFFFF`는 allocator sentinel로 추정되지만, 이것이 종료 node인지 실패 marker인지는 주변 read/write 관계로 계속 검증해야 한다.

## Shadow state와 shadow memory

shadow state는 실제 hardware/host state를 직접 변경하지 않고 guest가 보아야 할 상태를 별도 자료구조에 보존하는 기법이다. rePIU의 guest segment selector가 예다.

shadow memory는 원래 주소 공간을 직접 map할 수 없거나 instrumentation metadata가 필요할 때 별도 map에 byte/value를 보존하는 기법이다. 동적 분석 도구에서도 널리 쓰이며, 예를 들어 LLVM AddressSanitizer는 application memory 상태를 표현하기 위해 shadow memory를 사용한다. 구조 설명은 [Clang AddressSanitizer 문서](https://clang.llvm.org/docs/AddressSanitizer.html)를 참고할 수 있다.

rePIU의 shadow memory는 sanitizer와 목적이 다르다. arena 밖 guest store를 제한적으로 보존하여 후속 original-code read에 돌려주는 HLE 안전망이다.

## Frontier와 boundary object

frontier는 현재까지 연속성이 확인된 영역의 끝이다. boundary object는 실제 arena 끝을 걸쳐 배치된 객체다. 다음 object base가 frontier와 정확히 같을 때만 chain을 연장하면 임의의 out-of-range access를 정상 memory로 오인할 위험을 줄일 수 있다.

# Arena, Sentinel, and Shadow-Memory Terminology

An arena is a large region used to contain or serve multiple allocations. A sentinel is a reserved value or object representing a special state such as end, empty, failure, or boundary. Its exact meaning is data-structure-specific.

Shadow state preserves guest-visible state separately from host hardware state. Shadow memory stores values or metadata in a parallel mapping; [Clang AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html) is a well-known but differently purposed example. rePIU uses shadow memory narrowly to preserve analyzed guest stores outside the real arena.
