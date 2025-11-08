#pragma once

#include "nes_cpu.h"
#include <cstdint>

class Cartridge
{
public:
    void addMemory(uint32_t base, uint8_t* mem, uint32_t size)
    {
        memBase = base;
        memory = mem;
        memSize = size;
    }

    bool cpuRead(uint16_t addr, uint8_t& data)
    {
        if (addr >= memBase && addr < (memBase + memSize))
        {
            data = memory[addr - memBase];
            return true;
        }
        else if (addr == 0xFFFC) // Example special case
        {
            data = 0x10;
            return true;
        }
        else if (addr == 0xFFFD) // Example special case
        {
            data = 0xFE;
            return true;
        }
        return false;
    }

	bool cpuWrite(uint16_t addr, uint8_t data)
    {
        if (addr >= memBase && addr < (memBase + memSize))
        {
            memory[addr - memBase] = data;
            return true;
        }
        return false;
    }

private:
    uint32_t memBase;
    uint8_t* memory;
    uint32_t memSize;
};

class Bus
{
public:
    Cpu6502 cpu;
    Cartridge* cart;

    Bus();
    ~Bus();
	uint8_t cpuRead(uint16_t addr);
	void cpuWrite(uint16_t addr, uint8_t data);
    void OAM_Write(uint8_t addr, uint8_t data) { }

private:
    uint8_t RAM[2048];
    uint8_t controller; 
    uint8_t controller_state;
    uint8_t controller_strobe = 0x00;    
};
