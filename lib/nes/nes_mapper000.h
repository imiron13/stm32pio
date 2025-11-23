#ifndef MAPPER000_H
#define MAPPER000_H

#include "nes_mapper.h"

struct Mapper000_state
{
    Cartridge* cart;
    uint8_t number_PRG_banks;
    uint8_t number_CHR_banks;
    const uint8_t* CHR_bank;
    const uint8_t* PRG_banks[2];
};

Mapper createMapper000(uint8_t PRG_banks, uint8_t CHR_banks, Cartridge* cart);

#endif