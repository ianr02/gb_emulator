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
| `oam_bug.gb` | Passes |
| `dmg_sound.gb` | Passes `01`–`08`, `11` (Blargg APU suite) |

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

## Known Issues

Some games leave sound channels enabled (with an increasing volume envelope and no
length counter) during silent interludes, causing a steady tone instead of silence.
This is heard, for example, on Tetris's title/copyright screen before the theme and in
quiet moments throughout Kirby's Dream Land.
