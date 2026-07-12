# AOT 변환 계획 prototype 작업 지시

1. 플랫폼 공용 AOT plan 자료구조와 Zydis CFG analyzer를 구현합니다.
2. DOS/4GW loader 결과의 mapped image와 entry를 analyzer에 연결합니다.
3. `repiu_aot_probe` CLI를 추가합니다.
4. PIU 및 OpenWatcom sample에서 계획 시간과 분류 coverage를 측정합니다.
5. Win32 x86 build와 기존 analyzer regression을 확인합니다.
6. analysis/architecture/work log를 갱신하고 커밋합니다.

# AOT Translation Plan Prototype Work Order

Implement a platform-neutral AOT plan and Zydis CFG analyzer, add a DOS/4GW `repiu_aot_probe`, measure PIU and OpenWatcom coverage and planning time, verify Win32 x86, document the evidence, and commit the prototype.
