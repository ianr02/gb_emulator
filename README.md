# Game Boy Emulator

A Game Boy emulator written in C with an SDL2 frontend.

## Emulated Components

- **CPU** — SM83 instruction set, interrupts, and timers (working)
- **PPU** — graphics and rendering pipeline (working)
- **APU** — audio (in progress)
- **Memory / Bus / DMA / OAM** (working)
- **Cartridges** — MBC1, MBC2, MBC3 (incl. RTC), MBC5, ROM-only (working)

## Compatibility

Passes blargg's test ROMs:

- `cpu_instrs`
- `instr_timing`
- `mem_timing`
- `mem_timing2`
- `interrupt_time`

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
