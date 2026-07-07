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
