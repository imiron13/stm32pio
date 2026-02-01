#pragma once

template<int PREFETCH_SIZE>
class Prefetcher
{
    uint32_t base_address = 0;
    uint8_t buffer[PREFETCH_SIZE] __attribute__((aligned(4)));
    uint32_t hit = 0;
    uint32_t miss = 0;
public:
    void reset(uint32_t addr)
    {
        base_address = addr;
        for (int i = 0; i < PREFETCH_SIZE; i++)
            buffer[i] = 0;
    }

    void printStats()
    {
        printf("Prefetcher stats: hits=%lu, miss=%lu, hitrate=%lu%%" ENDL, hit, miss, (hit + miss) ? (100 * hit / (hit + miss)) : 0);
    }

    void prefetch(Bus* bus, uint32_t addr)
    {
        //if (addr >= base_address && addr < (base_address + PREFETCH_SIZE))
        //    return;

        base_address = addr & ~0x03;
        bus->cpuReadBlock(base_address, PREFETCH_SIZE, buffer);
    }

    void resetStats()
    {
        hit = 0;
        miss = 0;
    }

    uint8_t read(Bus* bus, uint32_t addr)
    {
        #if 0
        return bus->cpuRead(addr);
        #else
        if (likely(addr >= base_address && addr < (base_address + PREFETCH_SIZE - 3)))
        {
            //hit++;
            return buffer[addr - base_address];
        }
        else
        {
            //miss++;
            prefetch(bus, addr);
            return buffer[addr - base_address];
        }
        #endif
    }

    uint8_t readDirect(uint32_t addr)
    {
        return buffer[addr - base_address];
    }

    uint16_t readDirect16(uint32_t addr)
    {
        return *(uint16_t*)&buffer[addr - base_address];
    }
};
