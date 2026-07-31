#ifndef OAM_H
#define OAM_H

#include <stdint.h>
#include <stdbool.h>

uint8_t oam_read(uint16_t addr);
void oam_write(uint16_t addr, uint8_t val);
void oam_bug_incdec(void);
void oam_bug_read_inc(void);
void oam_set_read_inc(bool on);

#endif
