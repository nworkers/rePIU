# DOS4G HLE Porting Plan

## Objective

Create a native Win32 application capable of executing the original DOS/4G game while preserving gameplay behavior as accurately as possible.

This is **not** a source-code port.

This is **not** a DOSBox-based solution.

The project should execute the original protected-mode game code and replace only DOS services using High Level Emulation.

---

# Overall Architecture

```
Win32 Host

    |
    +-- LE/LX Loader
    |
    +-- Memory Manager
    |
    +-- Original Game Code
    |
    +-- HLE Layer
    |      |
    |      +-- DOS INT21
    |      +-- DPMI INT31
    |      +-- VGA
    |      +-- Keyboard
    |      +-- Mouse
    |      +-- Timer
    |      +-- Audio
    |      +-- File System
    |
    +-- SDL2 / Win32 Backend
```

---

# Important Principles

The original executable is considered the authoritative implementation.

Game logic should never be rewritten unless absolutely required.

Whenever possible:

* execute original functions
* emulate surrounding services
* avoid altering gameplay code

---

# Development Stages

## Stage 1

Executable Analysis

Tasks

* Detect DOS4GW / DOS4G / DOS32A executable
* Locate LE/LX header
* Parse object table
* Parse page table
* Parse relocation entries
* Build executable memory image

Deliverable

* Able to locate Entry Point.

---

## Stage 2

Memory System

Tasks

* Flat 32-bit memory
* Heap manager
* Stack initialization
* Selector abstraction
* Memory protection

Deliverable

* Original executable mapped successfully.

---

## Stage 3

Minimal HLE

Implement only the APIs actually used by the game.

Priority

1. File Open
2. File Read
3. File Seek
4. File Close
5. Memory Allocation
6. Time
7. Exit

Do not implement unnecessary DOS functionality.

---

## Stage 4

DPMI

Implement only required INT31 functions.

Expected minimum support

* Memory allocation
* Memory free
* Descriptor allocation
* Interrupt vector
* Exception vector

Expand only when required.

---

## Stage 5

Graphics

Target

* VGA Mode 13h
* Linear framebuffer
* Palette support

Internally convert framebuffer into SDL texture.

---

## Stage 6

Input

Support

* Keyboard
* Mouse

Convert native input events into expected DOS state.

---

## Stage 7

Timing

Implement

* PIT timer abstraction
* Fixed update rate
* Tick counter

Avoid frame-rate dependent gameplay.

---

## Stage 8

Audio

Support only features used by the game.

Possible implementations

* Sound Blaster DSP HLE
* CD Audio replacement
* WAV / FLAC playback

---

# Reverse Engineering Policy

Reverse engineering should identify:

* Main loop
* Rendering
* Input
* Resource loading
* Audio
* Save system

Document every discovered subsystem.

Avoid decompiling the entire executable unless necessary.

---

# Coding Rules

* Modern C++20
* Clear subsystem separation
* No global state where avoidable
* Logging for every HLE service
* Deterministic execution preferred

---

# Future Extensions

Possible future work

* Save states
* Debug overlay
* Memory inspector
* Instruction tracing
* Scriptable HLE hooks
* Optional CPU emulation backend

---

# Non-Goals

This project intentionally avoids:

* DOSBox integration
* Full PC emulation
* BIOS emulation
* Generic DOS compatibility

The implementation is game-oriented rather than a universal DOS runtime.

---

# Success Criteria

The project is considered successful when:

1. Original executable is loaded.
2. Original protected-mode code executes.
3. Game reaches the title screen.
4. Gameplay matches the original.
5. Rendering, input, and audio are supplied through the HLE layer.
6. The original executable remains the primary source of gameplay logic.
