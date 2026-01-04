#pragma once

#include "nes_cpu.h"
#include "nes_ppu.h"
#include "nes_cartridge.h"
#include <cstdint>

/*class Cartridge
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
*/

class Bus
{
public:
    uint32_t num_displayed_frames = 0;
    uint32_t num_skipped_frames = 0;
    uint32_t total_reads = 0;
    uint32_t total_writes = 0;
    uint32_t ram_reads = 0;
    uint32_t ram_writes = 0;

    enum CONTROLLER
    {
        A = (1 << 0), // A Button
        B = (1 << 1), // B Button
        Select = (1 << 2), // Select Button
        Start = (1 << 3), // Start Button
        Up = (1 << 4), // Up Button
        Down = (1 << 5), // Down Button
        Left = (1 << 6), // Left Button
        Right = (1 << 7)  // Right Button
    };

    Cpu6502 cpu;
    Ppu2C02 ppu;
    Cartridge* cart;
    uint16_t ppu_scanline = 0;
    uint8_t controller; 

    Bus();
    ~Bus();
    void insertCartridge(Cartridge* cartridge);
    void reset();
    void clock() optimize_speed;
	uint8_t cpuRead(uint16_t addr)
    {
        uint8_t data = 0x00;
        //total_reads++;
        if (likely((addr & 0xE000) == 0x0000))
        {   
            //ram_reads++;
            data = RAM[addr & 0x07FF];
        }
        else
        {
            data = cpuReadNonRam(addr);
        }
        return data;
    }
    uint8_t cpuStackRead(uint8_t ofs)
    {
        return ram_page1[ofs];
    }
    uint16_t cpuStackRead16(uint8_t ofs)
    {
        return *(uint16_t*)&ram_page1[ofs];
    }

    uint8_t cpuReadNonRam(uint16_t addr);
    void cpuReadBlock(uint16_t addr, uint32_t size, uint8_t* data);
    void cpuWrite(uint16_t addr, uint8_t data)
    {
        //total_writes++;
        if (likely((addr & 0xE000) == 0x0000))
        {
            //ram_writes++;
            RAM[addr & 0x07FF] = data;
        }
        else
        {
            cpuWriteNonRam(addr, data);
        }
    }

    void cpuStackWrite(uint8_t ofs, uint8_t data)
    {
        ram_page1[ofs] = data;
    }
    void cpuStackWrite16(uint8_t ofs, uint16_t data)
    {
        *(uint16_t*)&ram_page1[ofs] = data;
    }
    void cpuWriteNonRam(uint16_t addr, uint8_t data);
    void setPPUMirrorMode(Cartridge::MIRROR mirror);
    Cartridge::MIRROR getPPUMirrorMode();
    void OAM_Write(uint8_t addr, uint8_t data);
    void IRQ();
    void NMI();
    void renderImage(uint16_t scanline);

private:
    uint8_t RAM[2048];
    uint8_t *ram_page1 = &RAM[0x100]; // 0x0100-0x01FF stack page
    uint8_t controller_state;
    uint8_t controller_strobe = 0x00;    
};
