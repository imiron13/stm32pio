#include "nes_mapper.h"
#include "nes_cartridge.h"

#define IRAM_ATTR

IRAM_ATTR void bankInit(BankCache* cache, Bank* banks, uint8_t num_banks, uint32_t bank_size, Cartridge* cart)
{
    cache->banks = banks;
    cache->num_banks = num_banks;
    cache->tick = 0;
    cache->cart = cart;

    for (int i = 0; i < num_banks; i++)
    {
        cache->banks[i].bank_id = 0xFF;
        cache->banks[i].last_used = 0;
        cache->banks[i].size = bank_size;

        const uint8_t* ptr = cache->cart->loadPRGBank(bank_size, i * bank_size);
        #ifdef DEBUG
            if (!ptr) Serial.printf("%i KB for bank %d Allocation failed.\n", bank_size / 1024, i);
            else
            {
                Serial.printf("Allocated %i KB for bank %d, free heap: %u bytes\n",
                bank_size / 1024, i, heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
            }
        #endif
        cache->banks[i].bank_ptr = ptr;
    }
}

IRAM_ATTR const uint8_t* getBank(BankCache* cache, uint8_t bank_id, Mapper::ROM_TYPE rom)
{
    cache->tick++;

    for (int i = 0, n = cache->num_banks; i < n; i++)
    {
        if (cache->banks[i].bank_id == bank_id)
        {
            cache->banks[i].last_used = cache->tick;
            return cache->banks[i].bank_ptr;
        }
    }

    int bank_index = 0;
    uint32_t min_use = cache->banks[0].last_used;
    for (int i = 1, n = cache->num_banks; i < n; i++)
    {
        if (cache->banks[i].last_used < min_use)
        {
            min_use = cache->banks[i].last_used;
            bank_index = i;
        }
    }

    uint32_t size = cache->banks[bank_index].size;
    if (rom == Mapper::ROM_TYPE::PRG_ROM) cache->banks[bank_index].bank_ptr = cache->cart->loadPRGBank(size, bank_id * size);
    else if (rom == Mapper::ROM_TYPE::CHR_ROM) cache->banks[bank_index].bank_ptr =cache->cart->loadCHRBank(size, bank_id * size);

    cache->banks[bank_index].bank_id = bank_id;
    cache->banks[bank_index].last_used = cache->tick;

    return cache->banks[bank_index].bank_ptr;
}

uint8_t getBankIndex(BankCache* cache, uint8_t* ptr)
{
    for (int i = 0, banks = cache->num_banks; i < banks; i++)
    {
        if (cache->banks[i].bank_ptr == ptr)
            return cache->banks[i].bank_id;
    }

    return 0;
}

void invalidateCache(BankCache* cache)
{
    if (!cache || !cache->banks) return;

    cache->tick = 0;
    for (int i = 0, n = cache->num_banks; i < n; i++)
    {
        cache->banks[i].bank_id = 0xFF;
        cache->banks[i].last_used = 0;
    }
}
