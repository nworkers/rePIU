#!/usr/bin/env python3
"""Reconstruct DOS/16M MZ/BW copy and relocation tables.

This is an independent parser. Open Watcom's exe16m.h and load16m.c are
format references only; no source from those files is incorporated here.
References:
  https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exe16m.h
  https://github.com/open-watcom/open-watcom-v2/blob/master/bld/wl/c/load16m.c
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path
from typing import Any


BW_HEADER_SIZE = 176
GDT_ENTRY_SIZE = 8
RESERVED_GDT_BYTES = 16 * GDT_ENTRY_SIZE
BSS_MARKER = 0x2000


class FormatError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise FormatError(message)


def u16(data: bytes, offset: int) -> int:
    require(0 <= offset <= len(data) - 2, f"u16 out of range at 0x{offset:X}")
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    require(0 <= offset <= len(data) - 4, f"u32 out of range at 0x{offset:X}")
    return struct.unpack_from("<I", data, offset)[0]


def align16(value: int) -> int:
    return (value + 15) & ~15


def hex_value(value: int, width: int = 0) -> str:
    return f"0x{value:0{width}X}"


def parse_mz(data: bytes) -> dict[str, Any]:
    require(data[:2] == b"MZ", "missing MZ signature")
    last_page_bytes = u16(data, 2)
    page_count = u16(data, 4)
    relocation_count = u16(data, 6)
    header_size = u16(data, 8) * 16
    relocation_table = u16(data, 0x18)
    declared_size = (page_count - 1) * 512 + (last_page_bytes or 512)
    require(header_size <= declared_size <= len(data), "invalid MZ declared size")
    require(relocation_table + relocation_count * 4 <= header_size,
            "MZ relocation table exceeds header")

    relocations: list[dict[str, Any]] = []
    for index in range(relocation_count):
        entry_file_offset = relocation_table + index * 4
        target_offset = u16(data, entry_file_offset)
        target_segment = u16(data, entry_file_offset + 2)
        load_image_offset = target_segment * 16 + target_offset
        target_file_offset = header_size + load_image_offset
        require(target_file_offset + 2 <= declared_size,
                f"MZ relocation {index} target exceeds load image")
        relocations.append({
            "index": index,
            "entry_file_offset": hex_value(entry_file_offset, 8),
            "target_segment": hex_value(target_segment, 4),
            "target_offset": hex_value(target_offset, 4),
            "load_image_offset": hex_value(load_image_offset, 8),
            "target_file_offset": hex_value(target_file_offset, 8),
            "original_word": hex_value(u16(data, target_file_offset), 4),
            "operation": "add_mz_load_segment",
        })

    return {
        "declared_file_size": hex_value(declared_size, 8),
        "header_size": hex_value(header_size, 8),
        "load_image_file_begin": hex_value(header_size, 8),
        "load_image_file_end": hex_value(declared_size, 8),
        "initial_cs": hex_value(u16(data, 0x16), 4),
        "initial_ip": hex_value(u16(data, 0x14), 4),
        "relocation_table_file_offset": hex_value(relocation_table, 8),
        "relocation_count": relocation_count,
        "relocations": relocations,
    }


def read_exp_name(data: bytes, header_offset: int) -> str:
    raw = data[header_offset + 112:header_offset + 176]
    return raw.split(b"\0", 1)[0].decode("ascii", errors="strict")


def parse_bw_module(data: bytes, header_offset: int, index: int) -> dict[str, Any]:
    require(data[header_offset:header_offset + 2] == b"BW",
            f"missing BW signature at 0x{header_offset:X}")
    require(header_offset + BW_HEADER_SIZE <= len(data), "truncated BW header")

    next_header = u32(data, header_offset + 28)
    first_reloc_selector = u16(data, header_offset + 18)
    initial_ip = u16(data, header_offset + 20)
    initial_cs = u16(data, header_offset + 22)
    gdt_image_size = u16(data, header_offset + 56) + 1
    first_selector = u16(data, header_offset + 58) or 0x80
    require(header_offset < next_header <= len(data), "invalid next BW header offset")
    require(gdt_image_size >= RESERVED_GDT_BYTES,
            "GDT image is smaller than reserved selector area")
    extra_gdt_bytes = gdt_image_size - RESERVED_GDT_BYTES
    require(extra_gdt_bytes % GDT_ENTRY_SIZE == 0, "unaligned GDT image size")
    require(first_reloc_selector >= first_selector, "relocation selector precedes groups")
    require((first_reloc_selector - first_selector) % GDT_ENTRY_SIZE == 0,
            "unaligned relocation selector")

    gdt_count = extra_gdt_bytes // GDT_ENTRY_SIZE
    group_count = (first_reloc_selector - first_selector) // GDT_ENTRY_SIZE
    require(group_count < gdt_count, "relocation GDT entry is missing")
    gdt_file_offset = header_offset + BW_HEADER_SIZE
    image_file_offset = gdt_file_offset + extra_gdt_bytes
    require(image_file_offset <= next_header, "BW GDT exceeds module")

    entries: list[dict[str, Any]] = []
    for entry_index in range(gdt_count):
        entry_offset = gdt_file_offset + entry_index * GDT_ENTRY_SIZE
        limit, address_low, address_high, access, reserved = struct.unpack_from(
            "<HHBBH", data, entry_offset)
        entries.append({
            "selector": first_selector + entry_index * GDT_ENTRY_SIZE,
            "entry_file_offset": entry_offset,
            "limit": limit,
            "address_low": address_low,
            "address_high": address_high,
            "access": access,
            "reserved": reserved,
        })

    copy_cursor = image_file_offset
    copies: list[dict[str, Any]] = []
    group_limits: dict[int, int] = {}
    for entry in entries[:group_count]:
        selector = entry["selector"]
        is_bss = bool(entry["reserved"] & BSS_MARKER)
        file_size = 0 if is_bss else entry["limit"] + 1
        source_begin = None if is_bss else copy_cursor
        source_end = None if is_bss else copy_cursor + file_size
        if not is_bss:
            require(source_end is not None and source_end <= next_header,
                    f"copy for selector 0x{selector:X} exceeds module")
            # DOS/16M aligns the module-relative load position. Bound BW
            # headers are not necessarily aligned at an absolute file offset.
            copy_cursor = header_offset + align16(source_end - header_offset)
        group_limits[selector] = entry["limit"]
        copies.append({
            "selector": hex_value(selector, 4),
            "gdt_entry_file_offset": hex_value(entry["entry_file_offset"], 8),
            "source_file_begin": None if source_begin is None else hex_value(source_begin, 8),
            "source_file_end": None if source_end is None else hex_value(source_end, 8),
            "copy_size": hex_value(file_size, 8),
            "memory_paragraphs": hex_value(entry["reserved"] & ~BSS_MARKER, 4),
            "limit": hex_value(entry["limit"], 8),
            "access": hex_value(entry["access"], 2),
            "kind": "bss" if is_bss else "file_copy",
        })

    reloc_entry = entries[group_count]
    require(reloc_entry["selector"] == first_reloc_selector,
            "relocation GDT selector mismatch")
    relocation_size = reloc_entry["limit"] + 1
    relocation_begin = copy_cursor
    relocation_end = relocation_begin + relocation_size
    require(relocation_end == next_header,
            f"relocation range does not end at next header: 0x{relocation_end:X}")

    cursor = relocation_begin
    blocks: list[dict[str, Any]] = []
    total_relocations = 0
    terminated = False
    while cursor + 4 <= relocation_end:
        raw_selector = u16(data, cursor)
        count = u16(data, cursor + 2)
        if raw_selector == 0 and count == 0:
            break
        block_offset = cursor
        cursor += 4
        require(cursor + count * 2 <= relocation_end, "truncated RSI-2 block")
        terminal = bool(raw_selector & 0x0002)
        selector = raw_selector & ~0x0002
        require(selector in group_limits,
                f"RSI-2 block targets unknown selector 0x{selector:X}")
        offsets = [u16(data, cursor + item * 2) for item in range(count)]
        require(all(offset <= group_limits[selector] for offset in offsets),
                f"RSI-2 offset exceeds selector 0x{selector:X} limit")
        cursor += count * 2
        total_relocations += count
        blocks.append({
            "block_file_offset": hex_value(block_offset, 8),
            "raw_selector": hex_value(raw_selector, 4),
            "selector": hex_value(selector, 4),
            "terminal": terminal,
            "count": count,
            "offsets": [hex_value(offset, 4) for offset in offsets],
        })
        if terminal:
            terminated = True
            break

    require(terminated, "RSI-2 stream has no terminal selector")
    padding = data[cursor:relocation_end]
    require(all(byte == 0 for byte in padding), "nonzero bytes after RSI-2 terminator")

    return {
        "index": index,
        "name": read_exp_name(data, header_offset),
        "header_file_offset": hex_value(header_offset, 8),
        "next_header_file_offset": hex_value(next_header, 8),
        "initial_cs": hex_value(initial_cs, 4),
        "initial_ip": hex_value(initial_ip, 4),
        "first_selector": hex_value(first_selector, 4),
        "first_relocation_selector": hex_value(first_reloc_selector, 4),
        "gdt_image_size": hex_value(gdt_image_size, 4),
        "program_image_file_offset": hex_value(image_file_offset, 8),
        "copy_records": copies,
        "relocation_table": {
            "selector": hex_value(first_reloc_selector, 4),
            "file_begin": hex_value(relocation_begin, 8),
            "file_end": hex_value(relocation_end, 8),
            "size": hex_value(relocation_size, 8),
            "format": "RSI-2",
            "block_count": len(blocks),
            "relocation_count": total_relocations,
            "padding_bytes": len(padding),
            "blocks": blocks,
        },
    }


def reconstruct(data: bytes) -> dict[str, Any]:
    mz = parse_mz(data)
    cursor = int(mz["declared_file_size"], 16)
    modules: list[dict[str, Any]] = []
    while cursor < len(data):
        module = parse_bw_module(data, cursor, len(modules))
        modules.append(module)
        next_cursor = int(module["next_header_file_offset"], 16)
        require(next_cursor > cursor, "BW chain did not advance")
        cursor = next_cursor
    require(cursor == len(data), "BW chain does not end at EOF")
    require(modules, "no BW modules found")

    return {
        "schema": "repiu.dos16m-resident-tables.v1",
        "source_size": len(data),
        "source_sha256": hashlib.sha256(data).hexdigest(),
        "format_references": [
            "https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exe16m.h",
            "https://github.com/open-watcom/open-watcom-v2/blob/master/bld/wl/c/load16m.c",
        ],
        "source_code_incorporated": False,
        "mz_resident": mz,
        "bw_modules": modules,
        "summary": {
            "mz_relocation_count": mz["relocation_count"],
            "bw_module_count": len(modules),
            "copy_record_count": sum(len(module["copy_records"]) for module in modules),
            "rsi2_block_count": sum(module["relocation_table"]["block_count"] for module in modules),
            "rsi2_relocation_count": sum(module["relocation_table"]["relocation_count"] for module in modules),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path, nargs="?")
    args = parser.parse_args()
    try:
        manifest = reconstruct(args.input.read_bytes())
    except (OSError, FormatError, UnicodeError) as error:
        parser.exit(1, f"error: {error}\n")
    rendered = json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"
    if args.output is None:
        print(rendered, end="")
    else:
        args.output.write_text(rendered, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
