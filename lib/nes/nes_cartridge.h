#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <cstdint>
#include "nes_mapper.h"
#include "nes_mapper000.h"
using namespace std;
/*#include <SD.h>
#include <SPI.h>
#include <vector>
#include "mappers/mapper001.h"
#include "mappers/mapper002.h"
#include "mappers/mapper003.h"
#include "mappers/mapper004.h"*/

class Bus;
class Cartridge
{
public:
    Cartridge(const uint8_t* irom_data, size_t irom_size);
    ~Cartridge();

    enum MIRROR
    {
        HORIZONTAL,
        VERTICAL,
        ONESCREEN_LOW,
        ONESCREEN_HIGH,
        HARDWARE
    };

    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuReadBlock(uint16_t addr, uint32_t size, uint8_t* data);
	bool cpuWrite(uint16_t addr, uint8_t data);
	bool ppuRead(uint16_t addr, uint8_t& data);
    const uint8_t* ppuReadPtr(uint16_t addr);
	bool ppuWrite(uint16_t addr, uint8_t data);
    void ppuScanline();
    void reset();
    
    const uint8_t* loadPRGBank(uint16_t size, uint32_t offset);
    const uint8_t* loadCHRBank(uint16_t size, uint32_t offset);
    void setMirrorMode(MIRROR mirror);
    Cartridge::MIRROR getMirrorMode();
    void connectBus(Bus* n) { bus = n; }
    void IRQ();

    /*void dumpState(File& state);
    void loadState(File& state);*/

    uint8_t hardware_mirror;
    uint8_t mirror = HORIZONTAL;
    uint32_t CRC32 = ~0U;

private:
	Bus* bus = nullptr;
    uint32_t prg_base;
    uint32_t chr_base;

    //File rom;
    const uint8_t* rom_data;
    size_t rom_size;
    Mapper mapper;
    uint8_t mapper_ID = 0;
	uint8_t number_PRG_banks = 0;
	uint8_t number_CHR_banks = 0;

    uint32_t crc32(const void* buf, size_t size, uint32_t seed = ~0U);
};

#endif
