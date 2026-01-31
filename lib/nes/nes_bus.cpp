#include "nes_apu.h"
#include "nes_bus.h"
#include "nes_cpu.h"
#include "intrinsics.h"
#include <cstdint>
#include <cstring>

#include <hal_wrapper_stm32.h>
#include <dma_gpio_pwm_stm32.h>
#include "st7735.h"

#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_spi.h"

using namespace std;

#define IRAM_ATTR

Bus::Bus()
{
    memset(RAM, 0, sizeof(RAM));
    cpu.connectBus(this);
    cpu.apu.connectBus(this);
    ppu.connectBus(this);
}

Bus::~Bus()
{
}

IRAM_ATTR void Bus::cpuWriteNonRam(uint16_t addr, uint8_t data)
{
    if (cart->cpuWrite(addr, data)) {}
    else if ((addr & 0xE000) == 0x2000)
    {
        ppu.cpuWrite(addr, data);
    }
    else if ((addr & 0xF000) == 0x4000 && (addr <= 0x4013 || addr == 0x4015 || addr == 0x4017))
    {
        cpu.apuWrite(addr, data);
    }
    else if (addr == 0x4014)
    {
        cpu.OAM_DMA(data);
    }
    else if (addr == 0x4016)
    {
        controller_strobe = data & 1;
        if (controller_strobe)
        {
            controller_state = controller;
        }
    }
}

IRAM_ATTR uint8_t Bus::cpuReadNonRam(uint16_t addr)
{
    uint8_t data = 0x00;
    if (cart->cpuRead(addr, data)) {}
    else if ((addr & 0xE000) == 0x2000)
    {
        data = ppu.cpuRead(addr);
    }
    else if (addr == 0x4016)
    {
        uint8_t value = controller_state & 1;
        if (!controller_strobe)
            controller_state >>= 1;
        data = value | 0x40;
    }
    return data;
}

void Bus::cpuReadBlock(uint16_t addr, uint32_t size, uint8_t* data)
{
    if (cart->cpuReadBlock(addr, size, data)) {}
    else
    {
        for (uint32_t i = 0; i < size; i++)
        {
            data[i] = cpuRead(addr + i);
        }
    }
}

#include "hal_wrapper_stm32.h"
#include "ili9341.h"

void setx(uint32_t x0, uint32_t x1)
{
    Ili9341::controlMode();
    Ili9341::setXWindow(x0, x1);
    Ili9341::memoryWrite();
    Ili9341::dmaMode();
}

void lcd_sync(uint8_t *buf, uint32_t scanline)
{
    Ili9341::waitTransferComplete();
    //setx(32, 32 + 255);
        
    if (scanline % 2 == 0)
    {
        Ili9341::setupCircularDoubleBufferMode(buf, SCANLINE_SIZE * SCANLINES_PER_BUFFER * 2 * sizeof(uint16_t));
    }
    Ili9341::restartCs(10);
    Ili9341::controlMode();
    Ili9341::setAddressWindow(32, scanline, 32 + 255, 239);
    Ili9341::dmaMode();

    Ili9341::startTransfer();
}

IRAM_ATTR void Bus::clock()
{
    bool frame_skip = !cpu.apu.isBufferHalfFull();
    if (frame_skip == 0) num_displayed_frames++;
    else num_skipped_frames++;

    ppu.write_buf_idx = 0;
    ppu.ptr_display = ppu.display_buffer[1];
    if (!frame_skip) {
        Ili9341::setAddressWindow(32, 0, 32 + 255, 239);
        Ili9341::setupCircularDoubleBufferMode((uint8_t*)&ppu.display_buffer[0], SCANLINE_SIZE * SCANLINES_PER_BUFFER * 2 * sizeof(uint16_t));
    }

    // 1 frame == 341 dots * 261 scanlines
    // Visible scanlines 0-239
    
    // Rendering 3 scanlines at a time because 1 CPU clock == 3 PPU clocks
    // and there's only 341 ppu clocks (dots) in a scanline, which is not divisible by 3.
    // Using a counter/for loop with += 341 & -= 3 is too big of a performance hit.
    // 1 scanline == ~113.67 CPU clocks, so for every 3 scanlines, two scanlines will have an extra CPU clock
    for (ppu_scanline = 0; ppu_scanline < 240; ppu_scanline += 3)
    {
        if (frame_skip == 0) { 
            ppu.renderScanline(ppu_scanline); 
            Ili9341::doDoubleBufferTransfer();
        }
        else { ppu.fakeSpriteHit(ppu_scanline); }

        cpu.clock(113);
        //cpu.apu.clock(113/2);

        if (frame_skip == 0) { 
            ppu.renderScanline(ppu_scanline + 1); 
            Ili9341::doDoubleBufferTransfer();
        }
        else { ppu.fakeSpriteHit(ppu_scanline + 1); }

        cpu.clock(114);
        //cpu.apu.clock(114/2);

        if (frame_skip == 0) { 
            ppu.renderScanline(ppu_scanline + 2); 
            Ili9341::doDoubleBufferTransfer();
        }
        else { ppu.fakeSpriteHit(ppu_scanline + 2);  }

        cpu.clock(114);
        cpu.apu.clock((113+114+114) / 2);
    }
    Ili9341::waitTransferComplete();
    //cpu.apu.clock(80/2);
    // Setup for the next frame
    // Same reason as scanlines 0-239, 2/3 of scanlines will have an extra CPU clock. 
    // Scanline 240
    //cpu.clock(113);

    // Scanline 241-261
    ppu.setVBlank();
    cpu.clock(2501);

    ppu.clearVBlank();
    //cpu.clock(114);
    //cpu.apu.clock(338/2*240);
    cpu.apu.clock((2501 + 80)/2);
}


IRAM_ATTR void Bus::setPPUMirrorMode(Cartridge::MIRROR mirror)
{
    ppu.setMirror(mirror);
}

Cartridge::MIRROR Bus::getPPUMirrorMode()
{
    return ppu.getMirror();
}

IRAM_ATTR void Bus::OAM_Write(uint8_t addr, uint8_t data)
{
    ppu.ptr_sprite[addr] = data;
}

#if 0
void Bus::connectScreen(TFT_eSPI* screen)
{
    ptr_screen = screen;
}
#endif

IRAM_ATTR void Bus::renderImage(uint16_t scanline)
{
    /*#ifndef TFT_PARALLEL
        ptr_screen->pushImageDMA(32, scanline, 256, SCANLINES_PER_BUFFER, ppu.ptr_display);
    #else
        ptr_screen->pushImage(32, scanline, 256, SCANLINES_PER_BUFFER, ppu.ptr_display);
    #endif*/
    //if (scanline >= 160) return;
    //ST7735_WriteData((uint8_t*)ppu.ptr_display, 256 * SCANLINES_PER_BUFFER * 2);
    //spi_dma_wait_ready();
    //spi_dma_start(ppu.display_buffer[ppu.write_buf_idx] + ppu.x / 2);
    //spi_dma_init(ppu.display_buffer[ppu.write_buf_idx] + ppu.x, 256 * SCANLINES_PER_BUFFER * 2);
#if 0
    HAL_StatusTypeDef st;
    do {
        st = HAL_SPI_Transmit_DMA(&hspi1, reinterpret_cast<uint8_t*>(ppu.display_buffer[ppu.write_buf_idx] /*ppu.ptr_buffer*/), 128 /*256*/ * SCANLINES_PER_BUFFER * 2);
    } while (st != HAL_OK);
#endif
    ppu.ptr_display = ppu.display_buffer[ppu.write_buf_idx];
    ppu.write_buf_idx ^= 1;
    ppu.ptr_buffer = ppu.display_buffer[ppu.write_buf_idx];

} 


void Bus::insertCartridge(Cartridge* cartridge)
{
    cart = cartridge;
    ppu.connectCartridge(cartridge);
    cart->connectBus(this);
}

void Bus::reset()
{
    //ptr_screen->fillScreen(TFT_BLACK);
	for (auto& i : RAM) i = 0x00;
    cart->reset();
	cpu.reset();
    cpu.apu.reset();
	ppu.reset();
}

IRAM_ATTR void Bus::IRQ()
{
    cpu.IRQ();
}

IRAM_ATTR void Bus::NMI()
{
    cpu.NMI();
}

#if 0
void Bus::saveState()
{
    if (!SD.exists("/states")) SD.mkdir("/states");
    uint32_t CRC32 = cart->CRC32;

    char CRC32_str[9];
    sprintf(CRC32_str, "%08X", CRC32);

    char filename[32];
    sprintf(filename, "/states/%s.state", CRC32_str);

    File state = SD.open(filename, FILE_WRITE);
    if (!state) return;

    // Header for verification - ANEMOIA + CRC32
    state.print("ANEMOIA");
    state.write((const uint8_t*)CRC32_str, 8);

    // Dump state
    state.write(RAM, sizeof(RAM));
    cpu.dumpState(state);
    ppu.dumpState(state);
    cart->dumpState(state);

    state.close();
}

void Bus::loadState()
{
    uint32_t CRC32 = cart->CRC32;

    char CRC32_str[9];
    sprintf(CRC32_str, "%08X", CRC32);

    char filename[32];
    sprintf(filename, "/states/%s.state", CRC32_str);
    if (!SD.exists(filename)) return;

    File state = SD.open(filename, FILE_READ);
    if (!state) return;

    // Verify header
    char header[8];
    char CRC[9];
    state.read((uint8_t*)&header, 7);
    header[7] = '\0';
    state.read((uint8_t*)&CRC, 8);
    CRC[8] = '\0';

    if (strcmp(header, "ANEMOIA") != 0)
    {
        state.close();
        return;
    }
    if (strcmp(CRC, CRC32_str) != 0)
    {
        state.close();
        return;
    }
    
    // Load state
    state.read(RAM, sizeof(RAM));
    cpu.loadState(state);
    ppu.loadState(state);
    cart->loadState(state);

    state.close();
}
#endif
