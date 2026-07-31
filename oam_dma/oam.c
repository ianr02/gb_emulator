#include "oam_dma/oam.h"
#include "core/emulator_core.h"
#include "ppu/ppu.h"

// OAM is treated by the PPU as 20 rows of 4 16-bit words. All operations
// on the internal 16-bit data bus, so corruption operates on words.

static bool read_inc_glitch = false;

// While set, OAM reads are treated as coinciding with a 16-bit register
// inc/dec (LD A,(HL+/-), first POP/RET read), producing the complex
// "read during increase/decrease" corruption pattern.
void oam_set_read_inc(bool on) {
    read_inc_glitch = on;
}

static uint16_t oam_word(unsigned row, unsigned word) {
    unsigned off = row * 8 + word * 2;
    return (uint16_t)(memory->oam[off] | (memory->oam[off + 1] << 8));
}

static void oam_set_word(unsigned row, unsigned word, uint16_t val) {
    unsigned off = row * 8 + word * 2;
    memory->oam[off]     = val & 0xFF;
    memory->oam[off + 1] = val >> 8;
}

// "Write corruption": word0 replaced with ((a^c)&(b^c))^c, remaining
// words copied from the preceding row. Never corrupts row 0.
static void oam_corrupt_write(uint8_t row) {
    if (row == 0xFF || row == 0) return;
    uint16_t a = oam_word(row, 0);
    uint16_t b = oam_word(row - 1, 0);
    uint16_t c = oam_word(row - 1, 2);
    oam_set_word(row, 0, (uint16_t)(((a ^ c) & (b ^ c)) ^ c));
    oam_set_word(row, 1, oam_word(row - 1, 1));
    oam_set_word(row, 2, oam_word(row - 1, 2));
    oam_set_word(row, 3, oam_word(row - 1, 3));
}

// "Read corruption": same as write corruption but word0 = b | (a & c).
static void oam_corrupt_read(uint8_t row) {
    if (row == 0xFF || row == 0) return;
    uint16_t a = oam_word(row, 0);
    uint16_t b = oam_word(row - 1, 0);
    uint16_t c = oam_word(row - 1, 2);
    oam_set_word(row, 0, (uint16_t)(b | (a & c)));
    oam_set_word(row, 1, oam_word(row - 1, 1));
    oam_set_word(row, 2, oam_word(row - 1, 2));
    oam_set_word(row, 3, oam_word(row - 1, 3));
}

// "Read during increase/decrease": complex three-row pattern (rows 4-18
// only), then a normal read corruption is applied regardless.
static void oam_corrupt_read_inc(uint8_t row) {
    if (row >= 4 && row <= 18) {
        uint16_t a = oam_word(row - 2, 0);
        uint16_t b = oam_word(row - 1, 0);
        uint16_t c = oam_word(row, 0);
        uint16_t d = oam_word(row - 1, 2);
        oam_set_word(row - 1, 0, (uint16_t)((b & (a | c | d)) | (a & c & d)));
        for (unsigned w = 0; w < 4; w++) {
            uint16_t v = oam_word(row - 1, w);
            oam_set_word(row, w, v);
            oam_set_word(row - 2, w, v);
        }
    }
    oam_corrupt_read(row);
}

// Trigger a "write corruption" from an increment/decrement of a 16-bit
// register whose value is inside OAM. Fires at the inc/dec M-cycle.
void oam_bug_incdec(void) {
    oam_corrupt_write(ppu_oam_row());
}

// Trigger the combined read + implied inc/dec corruption (LD A,(HL+/-),
// first read of POP/RET when SP is in OAM).
void oam_bug_read_inc(void) {
    oam_corrupt_read_inc(ppu_oam_row());
}

// A bus read/write is serviced by the PPU one M-cycle before the emulator
// sees it (read_byte/save_byte advance the clock first), so the row is
// looked up at m-1.
static uint8_t oam_access_row(void) {
    return ppu_oam_row_at(ppu_m_cycle() - 1);
}

uint8_t oam_read(uint16_t addr) {
    uint8_t row = oam_access_row();
    if (row != 0xFF) {
        if (read_inc_glitch) oam_corrupt_read_inc(row);
        else                 oam_corrupt_read(row);
    }
    uint8_t stat_mode = memory->io[_STAT - 0xFF00] & 0x03;
    if (stat_mode >= 2) return 0xFF;
    if (addr >= 0xFEA0 && addr <= 0xFEFF) return 0x00;
    return memory->oam[addr - 0xFE00];
}

void oam_write(uint16_t addr, uint8_t val) {
    uint8_t row = oam_access_row();
    if (row != 0xFF)
        oam_corrupt_write(row);
    uint8_t stat_mode = memory->io[_STAT - 0xFF00] & 0x03;
    if (addr >= 0xFEA0 && addr <= 0xFEFF) return;
    if (stat_mode >= 2) return;
    memory->oam[addr - 0xFE00] = val;
}
