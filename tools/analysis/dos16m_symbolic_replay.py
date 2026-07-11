#!/usr/bin/env python3
"""Symbolically replay DOS/16M MZ/BW loading with byte provenance."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any

import dos16m_resident_tables as tables


INT21_ROUTER_PREFIX = bytes.fromhex("80 FC FF 74")
INT21_ROUTER_JUMP = bytes.fromhex("2E FF A5 6A 06")
SECONDARY_DISPATCH_CALL = bytes.fromhex("2E FF 95")
INT21_PRIMARY_TABLE_OFFSET = 0x066A
INT21_PRIMARY_TABLE_ENTRIES = 0x68


def integer(value: str | int) -> int:
    return int(value, 16) if isinstance(value, str) else value


def read_word(image: bytearray, offset: int) -> int:
    tables.require(0 <= offset <= len(image) - 2,
                   f"replay word out of range at 0x{offset:X}")
    return struct.unpack_from("<H", image, offset)[0]


def write_word(image: bytearray, offset: int, value: int) -> None:
    tables.require(0 <= value <= 0xFFFF, "replay word overflow")
    struct.pack_into("<H", image, offset, value)


def replay_mz(data: bytes, manifest: dict[str, Any]) -> dict[str, Any]:
    mz = manifest["mz_resident"]
    file_begin = integer(mz["load_image_file_begin"])
    file_end = integer(mz["load_image_file_end"])
    image = bytearray(data[file_begin:file_end])
    events: list[dict[str, Any]] = []
    for relocation in mz["relocations"]:
        offset = integer(relocation["load_image_offset"])
        original = read_word(image, offset)
        tables.require(original == integer(relocation["original_word"]),
                       "MZ replay source differs from manifest")
        events.append({
            "index": relocation["index"],
            "image_offset": tables.hex_value(offset, 8),
            "source_file_offset": relocation["target_file_offset"],
            "original_word": tables.hex_value(original, 4),
            "expression": f"0x{original:04X} + L",
            "provenance": "mz_file_word",
        })
    return {
        "base_symbol": "L",
        "image_size": len(image),
        "entry_expression": (
            f"(L + {mz['initial_cs']}):{mz['initial_ip']}"
        ),
        "relocation_count": len(events),
        "relocations": events,
    }


def replay_bw(data: bytes, manifest: dict[str, Any]) -> list[dict[str, Any]]:
    results: list[dict[str, Any]] = []
    for module in manifest["bw_modules"]:
        images: dict[int, bytearray] = {}
        provenance: dict[int, list[int | None]] = {}
        copy_by_selector: dict[int, dict[str, Any]] = {}
        selector_map: dict[int, int] = {}

        for copy in module["copy_records"]:
            selector = integer(copy["selector"])
            limit = integer(copy["limit"])
            paragraphs = integer(copy["memory_paragraphs"])
            memory_size = max(limit + 1, paragraphs * 16, 1)
            image = bytearray(memory_size)
            source_map: list[int | None] = [None] * memory_size
            if copy["kind"] == "file_copy":
                begin = integer(copy["source_file_begin"])
                end = integer(copy["source_file_end"])
                payload = data[begin:end]
                tables.require(len(payload) == integer(copy["copy_size"]),
                               "BW copy payload size mismatch")
                image[:len(payload)] = payload
                source_map[:len(payload)] = range(begin, end)
            images[selector] = image
            provenance[selector] = source_map
            copy_by_selector[selector] = copy
            selector_map[selector] = selector

        relocation_events: list[dict[str, Any]] = []
        changed_count = 0
        for block in module["relocation_table"]["blocks"]:
            target_selector = integer(block["selector"])
            image = images[target_selector]
            source_map = provenance[target_selector]
            for offset_text in block["offsets"]:
                offset = integer(offset_text)
                original = read_word(image, offset)
                mapped = selector_map.get(original, original)
                write_word(image, offset, mapped)
                if mapped != original:
                    changed_count += 1
                relocation_events.append({
                    "target_selector": tables.hex_value(target_selector, 4),
                    "target_offset": tables.hex_value(offset, 4),
                    "source_file_offset": (
                        None if source_map[offset] is None
                        else tables.hex_value(source_map[offset], 8)
                    ),
                    "original_selector": tables.hex_value(original, 4),
                    "mapped_selector": tables.hex_value(mapped, 4),
                    "changed": mapped != original,
                })

        entry_selector = integer(module["initial_cs"])
        entry_offset = integer(module["initial_ip"])
        tables.require(entry_selector in images, "BW entry selector has no image")
        tables.require(entry_offset < len(images[entry_selector]),
                       "BW entry offset exceeds image")
        entry_source = provenance[entry_selector][entry_offset]
        tables.require(entry_source is not None, "BW entry has no file provenance")

        results.append({
            "module": module["name"],
            "selector_policy": "identity (OPT_ROTATE is clear)",
            "base_symbols": {
                tables.hex_value(selector, 4):
                    f"B[{module['name']},{selector:04X}]"
                for selector in sorted(images)
            },
            "image_sizes": {
                tables.hex_value(selector, 4): len(images[selector])
                for selector in sorted(images)
            },
            "entry_selector": tables.hex_value(entry_selector, 4),
            "entry_offset": tables.hex_value(entry_offset, 4),
            "entry_source_file_offset": tables.hex_value(entry_source, 8),
            "relocation_count": len(relocation_events),
            "changed_relocation_count": changed_count,
            "relocations": relocation_events,
        })
    return results


def locate_router(data: bytes, manifest: dict[str, Any]) -> dict[str, Any]:
    mz = manifest["mz_resident"]
    header_size = integer(mz["header_size"])
    mz_end = integer(mz["declared_file_size"])
    hits: list[int] = []
    cursor = header_size
    while True:
        cursor = data.find(INT21_ROUTER_PREFIX, cursor, mz_end)
        if cursor < 0:
            break
        window = data[cursor:cursor + 0x80]
        if INT21_ROUTER_JUMP in window:
            hits.append(cursor)
        cursor += 1
    tables.require(len(hits) == 1, "INT 21h router source is not unique")
    router_file_offset = hits[0]
    router_linear_offset = router_file_offset - header_size

    minimum_segment = max(0, (router_linear_offset - 0xFFFF + 15) // 16)
    maximum_segment = router_linear_offset // 16
    candidates: list[dict[str, Any]] = []
    for segment in range(minimum_segment, maximum_segment + 1):
        base_file_offset = header_size + segment * 16
        table_file_offset = base_file_offset + INT21_PRIMARY_TABLE_OFFSET
        table_end = table_file_offset + INT21_PRIMARY_TABLE_ENTRIES * 2
        if table_end > mz_end:
            continue
        targets = list(struct.unpack_from(
            f"<{INT21_PRIMARY_TABLE_ENTRIES}H", data, table_file_offset))
        valid_targets = 0
        for target in targets:
            target_file = base_file_offset + target
            if (header_size <= target_file < mz_end and
                    data[target_file] not in (0x00, 0xFF)):
                valid_targets += 1
        candidates.append({
            "segment": segment,
            "router_ip": router_linear_offset - segment * 16,
            "table_file_offset": table_file_offset,
            "targets": targets,
            "valid_target_count": valid_targets,
            "distinct_target_count": len(set(targets)),
        })

    candidates.sort(
        key=lambda item: (item["valid_target_count"],
                          item["distinct_target_count"]),
        reverse=True)
    tables.require(candidates, "no resident CS candidate")
    best = candidates[0]
    best_score = (best["valid_target_count"], best["distinct_target_count"])
    tables.require(sum(
        (item["valid_target_count"], item["distinct_target_count"]) == best_score
        for item in candidates) == 1,
        "resident CS candidate score is ambiguous")
    tables.require(best["valid_target_count"] == INT21_PRIMARY_TABLE_ENTRIES,
                   "best resident table has invalid targets")

    service_zero_ip = best["targets"][0]
    service_zero_file = header_size + best["segment"] * 16 + service_zero_ip
    call_offset = data.find(
        SECONDARY_DISPATCH_CALL,
        service_zero_file,
        min(service_zero_file + 0x40, mz_end))
    tables.require(call_offset >= 0, "secondary dispatch call was not found")
    secondary_table_offset = u16_at(data, call_offset + 3)
    secondary_table_file = (
        header_size + best["segment"] * 16 + secondary_table_offset)
    secondary_zero_ip = u16_at(data, secondary_table_file)

    return {
        "router_source_file_offset": tables.hex_value(router_file_offset, 8),
        "router_load_image_offset": tables.hex_value(router_linear_offset, 8),
        "candidate_count": len(candidates),
        "selected_runtime_cs_relative_segment": tables.hex_value(best["segment"], 4),
        "runtime_cs_expression": f"L + 0x{best['segment']:04X}",
        "router_runtime_ip": tables.hex_value(best["router_ip"], 4),
        "primary_table_offset": tables.hex_value(INT21_PRIMARY_TABLE_OFFSET, 4),
        "primary_table_source_file_offset": tables.hex_value(
            best["table_file_offset"], 8),
        "primary_table_valid_targets": best["valid_target_count"],
        "primary_table_distinct_targets": best["distinct_target_count"],
        "service_zero_primary_ip": tables.hex_value(service_zero_ip, 4),
        "service_zero_primary_source_file_offset": tables.hex_value(
            service_zero_file, 8),
        "secondary_table_offset": tables.hex_value(secondary_table_offset, 4),
        "secondary_table_source_file_offset": tables.hex_value(
            secondary_table_file, 8),
        "secondary_service_zero_ip": tables.hex_value(secondary_zero_ip, 4),
        "secondary_service_zero_source_file_offset": tables.hex_value(
            header_size + best["segment"] * 16 + secondary_zero_ip, 8),
        "selection_evidence": (
            "unique maximum valid/distinct jump-target score across all "
            "16:16 decompositions of the router source linear offset"
        ),
    }


def u16_at(data: bytes, offset: int) -> int:
    tables.require(0 <= offset <= len(data) - 2,
                   f"source word out of range at 0x{offset:X}")
    return struct.unpack_from("<H", data, offset)[0]


def trace_identification_call(data: bytes,
                              resident: dict[str, Any]) -> dict[str, Any]:
    """Replay the verified 16-bit service-zero path for FF00:0078."""
    primary_file = integer(resident["service_zero_primary_source_file_offset"])
    secondary_file = integer(
        resident["secondary_service_zero_source_file_offset"])
    tables.require(data[primary_file:primary_file + 4] ==
                   bytes.fromhex("80 4E 26 01"),
                   "primary handler no longer sets saved carry")
    tables.require(data[primary_file + 0x0A:primary_file + 0x0D] ==
                   bytes.fromhex("8B 7E 12"),
                   "primary handler no longer reads saved DX")
    tables.require(data[primary_file + 0x23:primary_file + 0x26] ==
                   bytes.fromhex("89 46 16"),
                   "primary handler no longer writes saved AX")
    tables.require(data[secondary_file:secondary_file + 2] ==
                   bytes.fromhex("3C 05"),
                   "secondary service zero no longer bounds AL")
    tables.require(data[secondary_file + 4:secondary_file + 7] ==
                   bytes.fromhex("B8 FF FF"),
                   "secondary failure result is no longer AX=FFFF")

    return {
        "input": {
            "EAX": "0x0000FF00",
            "EDX": "0x00000078",
        },
        "wrapper": {
            "entry_ip": "0x0846",
            "operations": ["PUSHFD", "operand-size PUSH CS", "PUSH 0",
                           "near CALL 0x0C9E"],
        },
        "saved_frame": [
            {"offset": "0x00", "field": "scratch_di"},
            {"offset": "0x02", "field": "SS"},
            {"offset": "0x04", "field": "DS"},
            {"offset": "0x06", "field": "ES"},
            {"offset": "0x08", "field": "DI"},
            {"offset": "0x0A", "field": "SI"},
            {"offset": "0x0C", "field": "BP"},
            {"offset": "0x0E", "field": "SP_before_PUSHA"},
            {"offset": "0x10", "field": "BX"},
            {"offset": "0x12", "field": "DX"},
            {"offset": "0x14", "field": "CX"},
            {"offset": "0x16", "field": "AX"},
            {"offset": "0x18", "field": "reserved_6_bytes"},
            {"offset": "0x1E", "field": "IRETD_EIP_low_from_call_return"},
            {"offset": "0x20", "field": "IRETD_EIP_high_zero"},
            {"offset": "0x22", "field": "IRETD_CS_dword"},
            {"offset": "0x26", "field": "IRETD_EFLAGS_dword"},
        ],
        "data_flow": [
            "AH=FF -> INC AH wraps to 00 -> primary table index 0",
            "saved DX=0078 -> AX=0078, DI=DH=00",
            "secondary table index 0 -> handler 08DD",
            "AL=78 > 05 -> AX=FFFF",
            "failure branch skips primary carry-clear instruction",
            "saved AX at BP+16 becomes FFFF",
        ],
        "output": {
            "EAX": "0x0000FFFF (upper 16 bits preserved from input)",
            "AL": "0xFF",
            "EDX": "0x00000078 (preserved)",
            "carry_flag": 1,
            "GS": "preserved client-data selector",
            "other_flags": "preserved except handler-defined carry",
        },
        "gs_evidence": (
            "GS is neither pushed, popped, nor written by wrapper 0846, "
            "router 0C9E, primary 08B4, secondary 08DD, or restore 0D18"
        ),
        "contract_interpretation": (
            "PIU observes nonzero AL=FF and the pre-existing DOS/4G GS; "
            "it does not require carry clear or the DOS/32A signature"
        ),
    }


def recover_private_environment(data: bytes,
                                manifest: dict[str, Any]) -> dict[str, Any]:
    linexe = next(
        (module for module in manifest["bw_modules"]
         if module["name"] == "LINEXE.EXP"),
        None)
    tables.require(linexe is not None, "LINEXE module is missing")
    data_copy = next(
        (copy for copy in linexe["copy_records"]
         if copy["selector"] == "0x0090"),
        None)
    tables.require(data_copy is not None, "LINEXE data selector is missing")
    data_begin = integer(data_copy["source_file_begin"])
    data_end = integer(data_copy["source_file_end"])

    private_selector_global = 0x1AB8
    private_selector = u16_at(data, data_begin + private_selector_global)
    module_offset = 0x059A
    module_selector = 0x0090
    module_file = data_begin + module_offset
    next_offset = u16_at(data, module_file)
    next_selector = u16_at(data, module_file + 2)
    name_offset = u16_at(data, module_file + 4)
    name_selector = u16_at(data, module_file + 6)
    export_count = u16_at(data, module_file + 0x10)
    export_offset = u16_at(data, module_file + 0x12)
    export_selector = u16_at(data, module_file + 0x14)
    tables.require(module_selector == name_selector == export_selector,
                   "LINEXE module pointers use unexpected selectors")

    def asciz(offset: int) -> str:
        start = data_begin + offset
        end = data.find(b"\0", start, data_end)
        tables.require(end >= 0, "unterminated LINEXE string")
        return data[start:end].decode("ascii", errors="strict")

    module_name = asciz(name_offset)
    tables.require(module_name == "LINEXE_LOADER",
                   "private module name mismatch")
    exports: list[dict[str, Any]] = []
    for index in range(export_count):
        entry_offset = export_offset + index * 8
        entry_file = data_begin + entry_offset
        symbol_offset = u16_at(data, entry_file)
        symbol_selector = u16_at(data, entry_file + 2)
        value_offset = u16_at(data, entry_file + 4)
        value_selector = u16_at(data, entry_file + 6)
        tables.require(symbol_selector == module_selector,
                       "export name selector mismatch")
        exports.append({
            "index": index,
            "entry_offset": tables.hex_value(entry_offset, 4),
            "entry_source_file_offset": tables.hex_value(entry_file, 8),
            "name": asciz(symbol_offset),
            "name_pointer": f"{symbol_selector:04X}:{symbol_offset:04X}",
            "value": f"{value_selector:04X}:{value_offset:04X}",
        })

    required_names = {
        "LINEXE_LOADMODULE",
        "LINEXE_FREEMODULE",
        "GETLOADTABLE",
        "GETLOADNAME",
    }
    required = {
        item["name"]: item["value"]
        for item in exports if item["name"] in required_names
    }
    tables.require(set(required) == required_names,
                   "required LINEXE exports are incomplete")

    insertion_pattern = bytes.fromhex(
        "A1 B8 1A 2B DB 8E C0 26 8B 47 42 26 8B 57 44")
    insertion_file = data.find(
        insertion_pattern,
        integer(linexe["program_image_file_offset"]),
        data_begin)
    tables.require(insertion_file >= 0,
                   "private root insertion routine was not found")
    root_store = data.find(
        bytes.fromhex("26 89 44 42 26 89 54 44"),
        insertion_file,
        min(insertion_file + 0x80, data_begin))
    tables.require(root_store >= 0, "private root store was not found")

    return {
        "client_data_selector": tables.hex_value(private_selector, 4),
        "selector_source": (
            f"LINEXE selector 0090 global +{private_selector_global:04X}h"
        ),
        "selector_source_file_offset": tables.hex_value(
            data_begin + private_selector_global, 8),
        "root_offset": "0x0042",
        "root_initial_value": f"{next_selector:04X}:{next_offset:04X}",
        "root_population_routine_source_file_offset": tables.hex_value(
            insertion_file, 8),
        "root_store_source_file_offset": tables.hex_value(root_store, 8),
        "root_populated_value": f"{module_selector:04X}:{module_offset:04X}",
        "module": {
            "record_pointer": f"{module_selector:04X}:{module_offset:04X}",
            "record_source_file_offset": tables.hex_value(module_file, 8),
            "next_pointer": f"{next_selector:04X}:{next_offset:04X}",
            "name": module_name,
            "name_pointer": f"{name_selector:04X}:{name_offset:04X}",
            "export_count": export_count,
            "export_table_pointer": f"{export_selector:04X}:{export_offset:04X}",
        },
        "required_exports": required,
        "exports": exports,
        "minimum_environment": {
            "selectors": ["0x0020 private root", "0x0090 LINEXE data",
                          "0x0080 LINEXE code or HLE call gates"],
            "root_far_pointer": "0020:0042 -> 0090:059A",
            "module_record": "0090:059A, 0x16-byte observed header",
            "export_entries": "15 entries x 8 bytes at 0090:0522",
        },
    }


def replay(data: bytes) -> dict[str, Any]:
    manifest = tables.reconstruct(data)
    bw = replay_bw(data, manifest)
    resident = locate_router(data, manifest)
    return {
        "schema": "repiu.dos16m-symbolic-replay.v1",
        "source_sha256": manifest["source_sha256"],
        "source_code_incorporated": False,
        "mz_state": replay_mz(data, manifest),
        "bw_states": bw,
        "resident_int21": resident,
        "identification_call_trace": trace_identification_call(data, resident),
        "private_environment": recover_private_environment(data, manifest),
        "summary": {
            "mz_relocation_count": manifest["summary"]["mz_relocation_count"],
            "bw_relocation_count": sum(item["relocation_count"] for item in bw),
            "bw_changed_relocation_count": sum(
                item["changed_relocation_count"] for item in bw),
            "all_entry_points_mapped": True,
            "resident_cs_uniquely_selected": True,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path, nargs="?")
    args = parser.parse_args()
    try:
        result = replay(args.input.read_bytes())
    except (OSError, tables.FormatError, UnicodeError) as error:
        parser.exit(1, f"error: {error}\n")
    rendered = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output is None:
        print(rendered, end="")
    else:
        args.output.write_text(rendered, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
