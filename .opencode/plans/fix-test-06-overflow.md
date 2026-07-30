# Fix Test 06 (06-overflow on trigger.s) Regression

## Root Cause

Test 06 is the CH1 sweep overflow test (`06-overflow on trigger.s`). The failure `06:01` means the first CRC comparison failed.

`trigger_ch1()` in `apu/apu.c` performs a **second overflow check** that should only exist in `clock_sweep()`.

Per the Pan Docs reference:
- **Trigger** (§7.1): compute new_freq = shadow ± (shadow >> shift); if >2047, disable channel
- **Sweep step** (§7.3): compute new_freq; if >2047, disable; else store/update; THEN compute second overflow check

The second check (checking if the *next* sweep step would overflow) is a sweep-step-only behavior. Adding it on trigger causes frequencies near the overflow boundary to be incorrectly flagged as overflow on the first trigger.

## Test expectations (shift=1):
- Frequency **1365** (`0x555`): immediately `1365 + 682 = 2047` (OK) → channel stays ON
- Second check (incorrect): `2047 + 1023 = 3070 > 2047` → channel OFF → WRONG

## Fix

In `apu/apu.c`, function `trigger_ch1()`, remove lines 135-142 (the second overflow check):

Current (buggy):
```c
} else {
    apu->ch1_sweep_shadow = new_freq & 0x7FF;
    memory->io[_NR13 - 0xFF00] = new_freq & 0xFF;
    memory->io[_NR14 - 0xFF00] = (memory->io[_NR14 - 0xFF00] & ~0x07) | ((new_freq >> 8) & 0x07);
    apu->ch1_freq_latched = apu->ch1_sweep_shadow;
    uint16_t f_temp = apu->ch1_sweep_shadow;          // ← REMOVE
    uint16_t d2 = f_temp >> shift;                     // ← REMOVE
    if (dir_sweep == 0) f_temp += d2;                  // ← REMOVE
    else                f_temp -= d2;                  // ← REMOVE
    if (f_temp > 2047) {                               // ← REMOVE
        apu->ch1_enabled = 0;                          // ← REMOVE
        memory->io[_NR52 - 0xFF00] &= ~1;             // ← REMOVE
    }                                                  // ← REMOVE
}
```

Fixed:
```c
} else {
    apu->ch1_sweep_shadow = new_freq & 0x7FF;
    memory->io[_NR13 - 0xFF00] = new_freq & 0xFF;
    memory->io[_NR14 - 0xFF00] = (memory->io[_NR14 - 0xFF00] & ~0x07) | ((new_freq >> 8) & 0x07);
    apu->ch1_freq_latched = apu->ch1_sweep_shadow;
}
```

## Verification

After fix:
1. Build: `cd build && make -j4`
2. Run test: `./build/emulator tests/dmg_sound.gb 2>/dev/null`
3. Check result: `xxd .saves/DMG_SOUND.sav | head -1` → should show `06:ok`
