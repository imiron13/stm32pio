#include "nes_apu.h"
#include "nes_bus.h"
#include "nes_cpu.h"
#include "intrinsics.h"
#include <cstdint>
#include <cstring>

#include <hal_wrapper_stm32.h>
#include "st7735.h"

#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_spi.h"

using namespace std;

#define IRAM_ATTR
#define FRAMESKIP

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
extern TIM_HandleTypeDef htim1;

#include "ili9341.h"

#define DISPLAY_PAUSE()   (TIM1->CR1 &= ~TIM_CR1_CEN)
#define DISPLAY_RESUME()  (TIM1->CR1 |= TIM_CR1_CEN)


/* Add this to your Initialization or Burst Macro */
#define CONFIGURE_BURST_MODE(pixels) do { \
    /* ... (Your existing setup) ... */ \
    TIM1->RCR = (pixels) - 1; \
    TIM1->CCR2 = 1; \
    TIM1->DIER |= TIM_DIER_CC2DE; \
    TIM1->DIER &= ~TIM_DIER_UDE; \
    TIM1->CR1 |= TIM_CR1_OPM; \
    \
    /* LOAD SHADOW REGISTERS */ \
    TIM1->EGR = TIM_EGR_UG; \
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE); \
    \
    /* THE FIX: ENABLE OUTPUTS MANUALLY */ \
    /* 1. Enable Channel 1 (CC1E) */ \
    TIM1->CCER |= TIM_CCER_CC1E; \
    /* 2. Enable Main Output (MOE) - Critical for TIM1/TIM8 */ \
    TIM1->BDTR |= TIM_BDTR_MOE; \
} while(0)

void set_window()
{
    Ili9341::controlMode();
    Ili9341::setAddressWindow(32, 0, 32 + 255, 239);
    Ili9341::dmaMode();
 }

void setx(uint32_t x0, uint32_t x1)
{
    Ili9341::controlMode();
    Ili9341::setXWindow(x0, x1);
    Ili9341::enableRamAccess();
    Ili9341::dmaMode();
}

uint32_t g_buf_addr;

void lcd_sync(uint32_t scanline)
{
/* --- SYNC PHASE (Consumer Check) --- */
        
        /* A. Wait for Previous Line to Finish */
        /* If the timer is still running (CEN=1), it means it hasn't finished */
        /* sending the previous 256 pixels. We wait here. */
        while (TIM1->CR1 & TIM_CR1_CEN);

        /* B. Check Buffer Safety (Optional but recommended) */
        /* Ensure DMA isn't reading the half we just wrote to. */
        /* (Use your Get_DMA_Buf_Idx logic here if needed) */
        //setx(32, 32 + 255);
        
    if (scanline % 2 == 0)
    {
        HAL_DMA_Abort(htim1.hdma[TIM_DMA_ID_CC2]);
        HAL_DMA_Start(htim1.hdma[TIM_DMA_ID_CC2], 
                    g_buf_addr, 
                    (uint32_t)&GPIOA->ODR, 
                    SCANLINE_SIZE * SCANLINES_PER_BUFFER * 4);
    }
    Ili9341::restartCs(10);
    Ili9341::controlMode();
    Ili9341::setAddressWindow(32, scanline, 32 + 255, 239);
    Ili9341::dmaMode();

    /* --- FIRE PHASE --- */
    
    /* Kick the Timer to send exactly 256 pixels */
    /* Because OPM is on, it will run 256 times, then clear CEN automatically. */
    TIM1->CR1 |= TIM_CR1_CEN;
}

IRAM_ATTR void Bus::clock()
{
    frame_latch = !cpu.apu.isBufferHalfFull();
    if (frame_latch == 0) num_displayed_frames++;
    else num_skipped_frames++;

    if (frame_latch == 0) {
        g_buf_addr = (uint32_t)ppu.display_buffer;
        ppu.write_buf_idx = 0;
        ppu.ptr_display = ppu.display_buffer[1];
        set_window();

        HAL_DMA_Abort(htim1.hdma[TIM_DMA_ID_CC2]);
        HAL_DMA_Start(htim1.hdma[TIM_DMA_ID_CC2], 
                    (uint32_t)ppu.display_buffer, 
                    (uint32_t)&GPIOA->ODR, 
                    SCANLINE_SIZE * SCANLINES_PER_BUFFER * 4);
    }

    // 1 frame == 341 dots * 261 scanlines
    // Visible scanlines 0-239
    
    // Rendering 3 scanlines at a time because 1 CPU clock == 3 PPU clocks
    // and there's only 341 ppu clocks (dots) in a scanline, which is not divisible by 3.
    // Using a counter/for loop with += 341 & -= 3 is too big of a performance hit.
    // 1 scanline == ~113.67 CPU clocks, so for every 3 scanlines, two scanlines will have an extra CPU clock
    for (ppu_scanline = 0; ppu_scanline < 240; ppu_scanline += 3)
    {
        #ifndef FRAMESKIP
        ppu.renderScanline(ppu_scanline);
        if (ppu_scanline > 0)
            lcd_sync(ppu_scanline - 1);
        if (ppu_scanline == 0)
        {
            CONFIGURE_BURST_MODE(256*2);
            lcd_sync(ppu_scanline);
        }
        #else
            if (frame_latch == 0) { 
                ppu.renderScanline(ppu_scanline); 
                if (ppu_scanline > 0)
                    lcd_sync(ppu_scanline - 1);

                if (ppu_scanline == 0)
                {
                    CONFIGURE_BURST_MODE(256*2);
                    lcd_sync(ppu_scanline);
                }
            }
            else { ppu.fakeSpriteHit(ppu_scanline); }
        #endif

        cpu.clock(113);
        //cpu.apu.clock(113/2);

        #ifndef FRAMESKIP
        ppu.renderScanline(ppu_scanline + 1);
        lcd_sync(ppu_scanline);
        #else
            if (frame_latch == 0) { 
                ppu.renderScanline(ppu_scanline + 1); 
                lcd_sync(ppu_scanline);
            }
            else { ppu.fakeSpriteHit(ppu_scanline + 1); }
        #endif
        cpu.clock(114);
        //cpu.apu.clock(114/2);

        #ifndef FRAMESKIP
        ppu.renderScanline(ppu_scanline + 2);
        lcd_sync(ppu_scanline + 1);
        #else
            if (frame_latch == 0) { 
                ppu.renderScanline(ppu_scanline + 2); 
                lcd_sync(ppu_scanline + 1);
            }
            else { ppu.fakeSpriteHit(ppu_scanline + 2);  }
        #endif
        cpu.clock(114);
        cpu.apu.clock((113+114+114) / 2);
    }
    while (TIM1->CR1 & TIM_CR1_CEN);
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

    //if (frame_latch == 0) frame_latch = 1;
    //else frame_latch = frame_latch - 1;
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

bool dma_busy = 0;

static inline void spi_dma_start(void *data)
{
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)data);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, /*128*/ 256 * SCANLINES_PER_BUFFER * 2/2);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
    dma_busy = 1;
}

static inline void spi_dma_wait_ready(void)
{
    if (!dma_busy) return;
    // DMA transfer complete
    while (!LL_DMA_IsActiveFlag_TC2(DMA1)) {}
    LL_DMA_ClearFlag_TC2(DMA1);

    // Must wait until SPI is fully idle
    while (LL_SPI_IsActiveFlag_BSY(SPI1)) {}

    // Required: disable channel so next CNDTR reload takes effect
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    dma_busy = 0;
}

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
