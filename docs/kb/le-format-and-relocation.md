# LE 실행 형식과 fixup/relocation

LE(Linear Executable)는 OS/2 및 DOS extender 생태계에서 사용된 segmented/object-based executable format이다. header는 object table, page map, entry point, stack object, fixup page/record table 등의 위치를 제공한다. Microsoft의 오래된 executable-format 자료는 [Microsoft PE/COFF specification 다운로드 페이지](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)에서 PE 중심으로 제공되므로, LE field 자체는 Open Watcom 도구의 parser와 원본 format 문서를 함께 대조해야 한다.

## Object와 page

LE image는 하나의 flat file blob을 그대로 load하는 대신 object별 virtual size와 page mapping을 가진다. loader는 page data를 object-relative 위치에 복사하고 zero-fill 영역을 보완한다.

## Fixup

fixup record는 source 위치에 어떤 target object/offset 또는 selector/pointer를 기록할지 설명한다. relocated host base에 이미지를 놓으면 internal linear pointer에 relocation delta를 반영해야 한다. 반면 DOS low-memory offset, port number, interrupt number 같은 값은 relocation 대상이 아니다.

## 16:16과 32-bit pointer

fixup source type에 따라 selector, 16:16 far pointer, 32-bit offset/linear pointer의 byte width와 의미가 다르다. 단순히 모든 4바이트 값을 base-adjust하면 데이터 상수와 외부 address contract를 손상시킨다.

# LE Format and Fixup/Relocation

LE is an object/page-oriented executable format used in OS/2 and DOS-extender ecosystems. Its header references object, page, entry, stack, and fixup tables. The [Microsoft executable-format documentation](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) is primarily PE/COFF-oriented, so LE fields must also be cross-checked against Open Watcom tooling and historical LE references.

Fixups describe source and target semantics. Internal image pointers need relocation adjustment when the host base changes; DOS low-memory offsets, interrupt numbers, ports, and ordinary constants do not.
