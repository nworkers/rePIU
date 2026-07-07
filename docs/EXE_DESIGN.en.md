# Executable Design Notes

## Purpose

This document accumulates structures discovered from original executable analysis and loader design decisions.

## Current Findings

Current findings for `MASTER\PIU_1ST\PIU.EXE`:

* The file starts with an `MZ` DOS header.
* The `e_lfanew` value in the `MZ` header is `0x2C90`.
* Offset `0x2C90` contains the `LE` signature.
* `DOS4GW.EXE` exists in the same directory, but the project direction is not to run DOS4GW as an external runtime. The loader should directly analyze the LE image and provide required DOS/DPMI services through HLE.

## Design Direction

* Separate executable analysis into an MZ parser and an LE parser.
* Manage version-specific executable paths and asset roots through target profiles.
* Before first execution, use a non-executing analysis tool to verify the entry point, object table, page table, and fixup information.

## Non-Executing Analysis Tool

The first C++ tool reads `PIU.EXE` without executing it and prints fixed MZ/LE header information.

The initial LE parser interprets these fixed fields:

* byte order
* word order
* CPU type
* OS type
* module flags
* module page count
* entry object/index
* entry offset
* stack object/index
* stack offset
* page size
* object table offset/count
* object page table offset
* fixup page table offset
* fixup record table offset
* data pages offset

Detailed parsing of the object table, page table, and fixup records is accumulated in later stages.

## LE Image Mapping Dry-Run

The observed LE object table in `PIU.EXE` contains four 24-byte records.

The page table uses 4-byte records. The first three bytes are interpreted as a big-endian data page number, and the final byte is interpreted as page flags.

`data_pages_offset` is used as an absolute file offset. Data page number 1 points to `data_pages_offset`, and N points to `data_pages_offset + (N - 1) * page_size`.

The current dry-run mapping creates one buffer per object using `virtual_size`, then copies referenced pages into that buffer. The final page and object end are clamped so copying does not exceed the virtual size.

This step does not interpret fixup records or apply relocations.

## LE Fixup Section Analysis

The LE fixup page table is interpreted as `page_count + 1` 32-bit little-endian offsets.

Each offset is relative to the start of the fixup record table.

The fixup record range for page N starts at `offset[N]` and ends immediately before `offset[N + 1]`.

This step only validates whether the ranges are monotonic, fit inside the fixup record table size, and how large each per-page fixup span is.

The variable-length fixup record structure and relocation application are handled in a later design step.

## LE Fixup Record First-Pass Decoding

Fixup records are variable length, so the current step first supports the internal reference forms observed in `PIU.EXE`.

The common record prefix is interpreted as `source_type`, `target_flags`, and a 16-bit `source_offset`.

Internal targets are interpreted as a 1-byte object number and a target offset. The target offset size is treated as 16-bit or 32-bit depending on the 32-bit offset flag in `target_flags`.

Unsupported flag combinations are counted as unsupported records rather than relocation failures.

## Internal Relocation Dry-Run

All current `PIU.EXE` fixup records decode as internal targets.

The internal relocation value is calculated as the target object's `relocation_base_address` plus `target_offset`.

The source location is converted to an offset inside the owning object buffer using the fixup record's page index and source offset.

The observed source kind `0x07` is handled as a 32-bit little-endian write. Other source kinds need more selector/pointer interpretation, so this step counts them as skipped.

Some records use source offsets that cannot be written directly in the current 4 KB page buffer model, so they are counted as source out-of-range skipped.

## Relocation Skipped Source Analysis

Skipped relocations are not assigned final semantics yet. The analyzer prints counts by source kind and first samples so the next design step can be based on observed data.

The required samples are the first unsupported source kind record and the first source out-of-range record.

Because source kind alone cannot distinguish high source flag meanings, the analyzer also prints counts by full source type.

Unsupported sources print the first sample for each kind so cases such as `source_type=0x13` and plain `source kind 0x05` can be separated.

This step writes only to the analysis LE image buffers, not to executable memory.

## DOS/4GW Loader Result

`Dos4gwLoadResult` is added so the analysis tool and the future runtime can use the same loading flow.

`Dos4gwLoadResult` groups the MZ header, LE header, mapped LE image, fixup section analysis result, fixup record decoding result, and relocation dry-run result.

`LoadDos4gwExecutable` first checks that the target profile format hint is `DOS4GW_LE`, then fills the result using the existing MZ/LE/image/fixup/relocation sequence.

## Runtime Memory Dry-Run

The runtime memory dry-run calculates a pre-execution runtime memory layout plan from `Dos4gwLoadResult`.

The current dry-run records each LE object's relocation base address and virtual size as a runtime object region.

The entry linear address is calculated from the entry object base plus entry offset.

The stack top linear address is calculated from the stack object base plus stack offset. A stack offset equal to the object size is considered valid because it can point to the end of the object.

The HLE reserve base is calculated by rounding the maximum end of all object regions up to a 4 KB boundary.

## Win32/x86 Runtime Memory Policy

The Win32/x86 execution policy reports direct execution capability and the required reserve address range from the runtime memory dry-run result.

Direct control transfer to the original 32-bit x86 entry point is supported only from a 32-bit host process.

In a 64-bit host process, direct entry calls are reported as unsupported, and a future 32-bit helper process or separate execution backend is required.

The preferred allocation base is the lowest base address among runtime object regions.

The required reserve size is the HLE reserve base minus the preferred allocation base.
