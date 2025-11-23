#include "nes_mapper000.h"
#include "nes_cartridge.h"

#define IRAM_ATTR

IRAM_ATTR bool mapper000_cpuRead(Mapper* mapper, uint16_t addr, uint8_t& data)
{
    if (addr < 0x8000) return false;

    Mapper000_state* state = (Mapper000_state*)mapper->state;
    uint16_t offset = addr & 0x3FFF;
    uint8_t bank_id = (addr >> 14) & 1;

    data = state->PRG_banks[bank_id][offset];
    return true;
}

IRAM_ATTR bool mapper000_cpuReadBlock(Mapper* mapper, uint16_t addr, uint32_t size, uint8_t* data)
{
    if (addr < 0x8000) return false;

    Mapper000_state* state = (Mapper000_state*)mapper->state;
    uint16_t offset = addr & 0x3FFF;
    uint8_t bank_id = (addr >> 14) & 1;

    for (uint32_t i = 0; i < size / 4; i++)
    {
        ((uint32_t*)data)[i] = ((const uint32_t*)(state->PRG_banks[bank_id]))[offset / 4 + i];
    }
    //memcpy(data, &state->PRG_banks[bank_id][offset], size);
    /*for (uint32_t i = 0; i < size; i++)
    {
        data[i] = state->PRG_banks[bank_id][offset + i];
    }*/
    return true;
}

IRAM_ATTR bool mapper000_cpuWrite(Mapper* mapper, uint16_t addr, uint8_t data)
{
	return false;
}

IRAM_ATTR bool mapper000_ppuRead(Mapper* mapper, uint16_t addr, uint8_t& data)
{
    if (addr > 0x1FFF) return false;

    Mapper000_state* state = (Mapper000_state*)mapper->state;
    data = state->CHR_bank[addr];
    return true;
}

IRAM_ATTR bool mapper000_ppuWrite(Mapper* mapper, uint16_t addr, uint8_t data)
{
    if (addr > 0x1FFF) return false;

    Mapper000_state* state = (Mapper000_state*)mapper->state;
    if (state->number_CHR_banks == 0)
    {
        // Treat as RAM
        //state->CHR_bank[addr] = data;
        return true;
    }

	return false;
}

IRAM_ATTR const uint8_t* mapper000_ppuReadPtr(Mapper* mapper, uint16_t addr)
{
    if (addr > 0x1FFF) return nullptr;
    
    Mapper000_state* state = (Mapper000_state*)mapper->state;
    return &state->CHR_bank[addr];
}

void mapper000_reset(Mapper* mapper)
{
    Mapper000_state* state = (Mapper000_state*)mapper->state;
    state->PRG_banks[0] = state->cart->loadPRGBank(16*1024, 0);
    if (state->number_PRG_banks > 1) state->PRG_banks[1] = state->cart->loadPRGBank(16*1024, 16*1024);
    else state->PRG_banks[1] = state->PRG_banks[0];
    state->CHR_bank = state->cart->loadCHRBank(8*1024, 0);
}

/*
void mapper000_dumpState(Mapper* mapper, File& state)
{
    Mapper000_state* s = (Mapper000_state*)mapper->state;

    if (s->number_CHR_banks == 0 && s->CHR_bank)
    {
        state.write(s->CHR_bank, 8*1024);
    }
}

void mapper000_loadState(Mapper* mapper, File& state)
{
    Mapper000_state* s = (Mapper000_state*)mapper->state;

    if (s->number_CHR_banks == 0 && s->CHR_bank)
    {
        state.read(s->CHR_bank, 8 * 1024);
    }
}
*/

const MapperVTable mapper000_vtable = 
{
    mapper000_cpuRead,
    mapper000_cpuReadBlock,
    mapper000_cpuWrite,
    mapper000_ppuRead,
    mapper000_ppuWrite,
    mapper000_ppuReadPtr,
    nullptr,
    mapper000_reset,
    /*mapper000_dumpState,
    mapper000_loadState,*/
};

Mapper000_state mapper0_state;

Mapper createMapper000(uint8_t PRG_banks, uint8_t CHR_banks, Cartridge* cart)
{
    Mapper mapper;
    mapper.vtable = &mapper000_vtable; 

    Mapper000_state* state = &mapper0_state;
    state->number_PRG_banks = PRG_banks;
    state->number_CHR_banks = CHR_banks;
    state->cart = cart;

    mapper.state = state;
    return mapper;
}