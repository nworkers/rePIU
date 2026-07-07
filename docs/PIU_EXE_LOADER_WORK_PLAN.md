# 이동 안내

이 문서는 새 문서 규칙 도입 전에 작성된 이전 위치의 계획서입니다.

현재 기준 작업 지시 문서는 `docs/work-orders/20260708-002-piu-exe-loader.md`입니다.

# Moved Notice

This document was written before the new documentation rules were introduced.

The current canonical work-order document is `docs/work-orders/20260708-002-piu-exe-loader.md`.

---

# PIU.EXE Loader Work Plan

## Objective

Build a native Win32 loader that can load and execute the original DOS/4G executable at:

`MASTER\PIU_1ST\PIU.EXE`

The loader must preserve the original game logic by executing the protected-mode x86 code from the executable. The surrounding DOS, DPMI, filesystem, graphics, input, timer, and audio services should be supplied by project-owned HLE layers.

This plan intentionally supports adding more game versions later. `PIU_1ST` is the first target, not a one-off hardcoded runtime.

## Current File Observations

Initial inspection of `MASTER\PIU_1ST\PIU.EXE` shows:

* The file begins with an `MZ` DOS header.
* The `e_lfanew` pointer at offset `0x3C` points to `0x2C90`.
* Offset `0x2C90` contains the `LE` signature.
* The target directory also contains `DOS4GW.EXE`, but the project should not run DOS4GW as a black box. The host should directly parse/load the protected-mode executable image and provide required HLE services.

This matches the Stage 1 direction in `docs/DOS4G_HLE_PORTING_PLAN.md`: detect DOS4G/DOS4GW, locate the LE header, parse executable structures, and locate the entry point.

## Non-Goals

* Do not reimplement gameplay logic in C++.
* Do not integrate DOSBox.
* Do not build a generic DOS runtime.
* Do not patch original executable code unless a later documented blocker proves it unavoidable.
* Do not make `MASTER\PIU_1ST` assumptions leak into the core loader.

## High-Level Architecture

The loader should be split into independently replaceable modules:

* `TargetRegistry`: discovers or selects a game target/version.
* `TargetProfile`: describes executable path, working directory, expected format, and version-specific metadata.
* `ExecutableReader`: reads original files without mutating them.
* `MzParser`: parses the DOS MZ header and locates the LE/LX image.
* `LeParser`: parses LE headers, object table, page table, fixups, imports, and entry point data.
* `ImageMapper`: builds the in-memory image expected by the original protected-mode code.
* `RuntimeMemory`: owns mapped executable memory, stack, heap, selector abstractions, and low-memory/HLE regions.
* `ExecutionEngine`: transfers control to original x86 code in a 32-bit Win32 process.
* `HleDispatcher`: handles DOS, DPMI, timer, input, graphics, audio, and filesystem calls.
* `TraceLogger`: records loader decisions, HLE calls, exceptions, and execution milestones.

The first implementation can be minimal, but these boundaries should be present early so future versions can be added by data/profile rather than by copying loader code.

## Build Constraint

Executing original 32-bit x86 code directly requires a 32-bit host process. The first native loader should therefore target Win32/x86, even when built on a 64-bit Windows machine.

If a future x64 host is required, it should use a separate execution backend such as a helper 32-bit process or optional CPU emulation. That is outside the first loader milestone.

## Target Profile Model

Start with a built-in profile:

```text
id: piu_1st
root: MASTER\PIU_1ST
executable: PIU.EXE
working_directory: MASTER\PIU_1ST
format_hint: DOS4G_LE
```

The profile system should be able to grow into:

* command-line selection, for example `repiu --target piu_1st`
* explicit executable path selection, for example `repiu --exe MASTER\PIU_1ST\PIU.EXE`
* per-version file hashes and compatibility notes
* per-version HLE quirks when required

## Milestone 1: Executable Analysis Tool

Create a command-line inspection path before trying to execute the game.

Required output:

* selected target id and root
* executable size and hash
* MZ header summary
* LE header offset and signature
* LE byte order, word order, executable type, CPU type, module flags
* object table offset/count
* page table offset/count
* fixup table offsets
* entry object and entry offset

Acceptance criteria:

* Running the tool against `MASTER\PIU_1ST\PIU.EXE` reliably identifies the LE image.
* The entry point can be reported without executing original code.
* Parser errors include file offsets and enough context to continue reverse engineering.

## Milestone 2: LE Image Mapping

Implement enough of the LE loader to construct the protected-mode image in memory.

Tasks:

* Parse object descriptors.
* Allocate memory for each object with correct size and flags.
* Load page data into object memory.
* Apply internal fixups.
* Detect unsupported external imports or loader records.
* Create an initial stack according to LE metadata or documented DOS4GW expectations.

Acceptance criteria:

* The image maps deterministically from `PIU.EXE`.
* Internal relocations are applied or explicitly reported as unsupported.
* The entry point resolves to a host-callable address inside a mapped object.
* No original executable bytes are modified on disk.

## Milestone 3: Minimal Runtime Shell

Create the minimum process/runtime state needed before first execution.

Tasks:

* Build a flat 32-bit address model compatible with the executable's assumptions.
* Reserve known HLE regions for low memory, interrupt stubs, DOS data, and runtime heap.
* Provide a selector abstraction even if the first build maps selectors to flat memory.
* Install SEH/VEH handling for exceptions generated by DOS/DPMI interrupt instructions or invalid host interactions.
* Add structured logging for every runtime transition.

Acceptance criteria:

* The loader can prepare runtime state and stop immediately before entry point transfer.
* A dry-run mode prints the planned entry address, stack address, and object map.

## Milestone 4: First Execution Transfer

Transfer control to original code in a controlled harness.

Tasks:

* Start execution at the LE entry point.
* Catch early faults with register dump and mapped-object lookup.
* Detect `int 21h`, `int 31h`, timer, file, and exit paths.
* Return cleanly to the host on game exit or controlled fatal error.

Important note:

The first execution attempt is expected to fail quickly. The goal is not immediate title screen success; the goal is to identify the first missing HLE contract while preserving original code execution.

Acceptance criteria:

* The host reaches the original entry point.
* The first unsupported HLE call or CPU fault is logged with registers, EIP object/offset, and nearby bytes.
* The host process remains debuggable and exits cleanly after failure.

## Milestone 5: Minimal DOS/DPMI HLE

Implement only services observed from `PIU.EXE`.

Initial priority:

* DOS file open/read/seek/close
* DOS current directory and path translation relative to the target root
* DOS memory allocation where applicable
* DPMI memory allocation/free
* DPMI descriptor allocation/query/set where actually used
* interrupt vector and exception vector calls
* time/tick queries
* process exit

Acceptance criteria:

* HLE logs show real game file access under `MASTER\PIU_1ST`.
* Missing services are added from traces, not from broad DOS compatibility guesses.
* File access is target-root aware and ready for multiple game versions.

## Milestone 6: Graphics/Input/Timer Bring-Up

Once file and DPMI calls progress far enough, add hardware-facing HLE.

Expected work:

* VGA mode/palette/framebuffer capture if the game uses VGA paths.
* Glide-related investigation because `glide2x.ovl` is present in the target directory.
* Keyboard state mapping.
* Timer/PIT tick behavior.
* Basic window output through Win32 or SDL backend.

Acceptance criteria:

* The game reaches a visible screen or a clearly documented next blocker.
* Rendering data path is original-code driven.
* Input and timing are supplied externally without rewriting game flow.

## Milestone 7: Version Scaling

After `PIU_1ST` reaches a meaningful milestone, generalize target support.

Tasks:

* Add manifest/profile file support.
* Store per-target hashes and known loader/HLE requirements.
* Add regression inspection for all configured targets.
* Keep shared loader code version-agnostic.

Acceptance criteria:

* Adding a new version requires adding a profile and only documented quirks.
* Existing `PIU_1ST` behavior remains covered by loader tests/traces.

## Recommended First Implementation Order

1. Add a small C++20 Win32 console host skeleton.
2. Add `TargetProfile` and built-in `piu_1st` target.
3. Add binary reader and MZ parser.
4. Add LE parser enough to print header/object/page/fixup metadata.
5. Add non-executing image map dry-run.
6. Add controlled entry transfer only after mapping and logging are solid.
7. Grow HLE strictly from observed execution traces.

## Logging Requirements

Every run should produce enough information to reproduce progress:

* selected target profile
* executable path and hash
* parser summaries
* object memory map
* applied relocation counts
* entry point
* HLE call traces
* exception/fault reports

Prefer plain text logs first. Binary traces can be added later if instruction-level debugging becomes necessary.

## Risks And Investigation Items

* LE fixup records may require more complete parsing than the first milestone predicts.
* The executable may depend on DOS4GW loader behavior not captured by the LE spec alone.
* Segment selector behavior may not map cleanly to a flat Win32 process and may need descriptor emulation or targeted thunking.
* Some `int` instructions may surface as Windows exceptions and require careful SEH/VEH handling.
* Graphics may use Glide through `glide2x.ovl`, which changes the graphics HLE priority from pure VGA to Glide/DOS extender interaction.
* Modern 64-bit host execution cannot directly call 32-bit game code in-process, so the direct execution backend must be Win32/x86.

## Immediate Next Task

Implement Milestone 1 as a non-executing analysis tool for `MASTER\PIU_1ST\PIU.EXE`.

That gives the project a safe foundation: before we run unknown original code, we can prove that the executable format, entry point, object layout, and relocation metadata are understood well enough to proceed.
