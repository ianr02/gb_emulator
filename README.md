# Game Boy Emulator

A Game Boy emulator written in C with an SDL2 frontend.

## Emulated Components

- **CPU** — SM83 instruction set, interrupts, and timers (working)
- **PPU** — graphics and rendering pipeline (working)
- **APU** — audio (in progress)
- **Memory / Bus / DMA / OAM** (working)
- **Cartridges** — MBC1, MBC2, MBC3 (incl. RTC), MBC5, ROM-only (working)

## Compatibility

| Test | Check |
|------|-------|
| `cpu_instrs.gb` | Passes |
| `instr_timing.gb` | Passes |
| `mem_timing.gb` | Passes |
| `mem_timing2.gb` | Passes |
| `interrupt_time.gb` | Passes (DMG portion; the test is designed for both DMG and GBC) |
| `halt_bug.gb` | Passes |

## Build

Requires [SDL2](https://github.com/libsdl-org/SDL).

```sh
mkdir build && cd build
cmake ..
make
```

## Run

```sh
./emulator <path_to_rom>
```

Battery saves are written to `.saves/`.

## Controls

| Game Boy | Keyboard  |
|----------|-----------|
| D-Pad    | Arrow keys|
| A        | Z         |
| B        | X         |
| Select   | Right Shift |
| Start    | Enter     |
| Quit     | Escape    |
