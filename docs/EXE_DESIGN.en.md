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
