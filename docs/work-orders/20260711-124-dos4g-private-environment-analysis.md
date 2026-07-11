# DOS/4G private environment 분석 작업 지시

1. 동일 DOS4GW 바이너리에서 `INT 21h AX=FF00h` provider handler를 찾는다.
2. 성공/실패 반환 register와 flag를 복원한다.
3. PIU의 `GS:0x42` root 및 node 순회를 완전 디스어셈블한다.
4. 최소 structure field map과 selector 요구사항을 작성한다.
5. 근거가 충분하면 최소 HLE 구현안을 제시하고, 부족하면 필요한 캡처 항목을 결정 지점으로 남긴다.

# DOS/4G Private Environment Analysis Work Order

Locate the `INT 21h AX=FF00h` provider handler in the identical DOS4GW binary, recover success and failure register/flag results, fully disassemble PIU's `GS:0x42` root and node traversal, produce the minimum field and selector map, and either propose an evidence-backed minimum HLE contract or leave the exact capture requirements as the next decision point.
